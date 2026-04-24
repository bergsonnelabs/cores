#!/usr/bin/env python3
"""
gen_tessera_natives.py — generate WAMR NativeSymbol table +
adapter wrappers from `manifests/sdk-docs/*.json`.

For every function with `tessera_exposed: true`, emits:

  1. A `static void|int|double core_X_native(wasm_exec_env_t, ...)`
     adapter that forwards from WAMR's calling convention (exec_env
     first, scalars after) to a direct call into the SDK.
  2. An entry in `g_tessera_natives[]` pairing the host symbol name
     with the adapter pointer and the WAMR signature string
     ("()", "(i)", "(iii)", "(F)", "(i)i", etc.).

The firmware embedder registers everything in one call:

  extern const NativeSymbol g_tessera_natives[];
  extern const size_t g_tessera_natives_count;
  wasm_runtime_register_natives("env", g_tessera_natives,
                                g_tessera_natives_count);

Running this is an SDK maintenance step, not a firmware-build step.
The generated files are checked into `sdk/wamr/` so embedders don't
need Python at build time. Re-run when manifests change (currently
hand-edited; long-term should flow through the `@tessera` annotation
generator).

Out-of-scope today:
  - Pointer parameters (`const char *`, arrays) — these need WAMR's
    `wasm_runtime_validate_app_addr` + `addr_app_to_native` dance
    and a per-import scheme. Skipped with a comment; the skipped
    set will come back when a real `Core.USB.print(string)` path
    is designed.
  - Hosts that return a pointer.
  - Functions without a `signature` entry in the manifest (rare;
    older manifests).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parent.parent
SDK_DOCS = REPO_ROOT / "manifests" / "sdk-docs"
DEFAULT_OUT_C = REPO_ROOT / "sdk" / "wamr" / "tessera_natives.c"
DEFAULT_OUT_H = REPO_ROOT / "sdk" / "wamr" / "tessera_natives.h"


# ---------------------------------------------------------------------------
# Type mapping
#
# Keep this table narrow on purpose. SDK ctypes that aren't here get the
# function skipped with a diagnostic — better to exclude loudly than to
# silently mis-lower. The C API's own types.h is authoritative; this mirror
# tracks it by hand for now.
# ---------------------------------------------------------------------------
I32_TYPES = {
    "int", "unsigned int", "bool",
    "int8_t", "int16_t", "int32_t",
    "uint8_t", "uint16_t", "uint32_t",
    "hal_status_t",
}

# Categories whose adapter wrappers reach into coregen-generated
# per-project state (handles like `core_adc1`, dispatchers like
# `core_pwm_timer_for_pad`). Those can only be wrapped once we
# know the project's config.json — a later per-project generation
# step. For now we exclude them from the cross-project catalog so
# `tessera_natives.c` links unconditionally.
#
# Safe categories: inline helpers that only reach into the ll_*
# layer (no config-generated globals) or into HAL code that's
# always compiled.
PROJECT_INDEPENDENT_CATEGORIES = {
    "led",
    "timing",
    "watchdog",
    "nvm",
    "usb",  # only core_usb_print_* stays — core_usb_print(string) is already skipped for pointer param
}
F64_TYPES = {"double"}
F32_TYPES = {"float"}  # WAMR uses 'f' for f32, 'F' for f64 in sig strings
POINTER_TYPES_SKIP = {"const char *", "char *", "void *", "uint8_t *"}


@dataclass
class HostFn:
    category: str
    name: str            # C symbol (e.g. "core_led_heartbeat")
    params: list[dict]   # each { "name", "ctype" }
    returns: str         # C return type; "void" if none

    @property
    def wamr_signature(self) -> str:
        pchars = "".join(wamr_char(p["ctype"]) for p in self.params)
        rt = self.returns.strip()
        if rt == "void":
            return f"({pchars})"
        return f"({pchars}){wamr_char(rt)}"

    @property
    def c_return(self) -> str:
        return self.returns.strip()

    def skip_reason(self) -> str | None:
        if self.category not in PROJECT_INDEPENDENT_CATEGORIES:
            return f"category '{self.category}' reaches into coregen-generated state; needs per-project generation"
        for p in self.params:
            ct = p.get("ctype", "").strip()
            if ct in POINTER_TYPES_SKIP:
                return f"pointer param '{ct}' not yet supported"
            if wamr_char(ct) is None:
                return f"param type '{ct}' not in the i32/f32/f64 map"
        rt = self.c_return
        if rt != "void" and wamr_char(rt) is None:
            return f"return type '{rt}' not in the i32/f32/f64 map"
        return None


def wamr_char(ctype: str) -> str | None:
    t = ctype.strip()
    if t in I32_TYPES:
        return "i"
    if t in F32_TYPES:
        return "f"
    if t in F64_TYPES:
        return "F"
    return None


def c_param_type(ctype: str) -> str:
    """Adapter-side param type. i32s come in as int32_t, floats as
    double (WAMR promotes f32 native args to f64 on the C boundary for
    variadic safety — but the adapter just forwards, so we use the
    manifest's declared ctype directly)."""
    return ctype.strip()


def load_manifests(sdk_docs: Path) -> list[HostFn]:
    out: list[HostFn] = []
    for path in sorted(sdk_docs.glob("*.json")):
        data = json.loads(path.read_text())
        cat = data.get("category", path.stem)
        for fn in data.get("functions", []):
            if not fn.get("tessera_exposed"):
                continue
            out.append(HostFn(
                category=cat,
                name=fn["name"],
                params=[{"name": p.get("name", f"_p{i}"),
                         "ctype": p.get("ctype", "int")}
                        for i, p in enumerate(fn.get("params", []))],
                returns=fn.get("returns", "void"),
            ))
    return out


# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------

WARNING = (
    "/* ---------------------------------------------------------------------\n"
    " * Generated by tools/gen_tessera_natives.py - DO NOT EDIT BY HAND.\n"
    " * Regenerate when manifests/sdk-docs/ JSON files change.\n"
    " * ------------------------------------------------------------------ */\n"
)


def emit_wrapper(fn: HostFn) -> str:
    """A `static` wrapper that matches WAMR's native-call convention
    and forwards to the real SDK symbol. Marked `static` so LTO +
    gc-sections can drop wrappers for hosts the user's Wasm module
    never actually imports — the NativeSymbol table entry keeps
    them reachable from the register path, but `register_natives`
    only validates signatures, it doesn't force-link them."""
    sig = fn.wamr_signature
    rt = fn.c_return
    params = fn.params

    if not params:
        param_decl = "wasm_exec_env_t env"
    else:
        parts = ["wasm_exec_env_t env"]
        for p in params:
            parts.append(f"{c_param_type(p['ctype'])} {p['name']}")
        param_decl = ", ".join(parts)

    call_args = ", ".join(p["name"] for p in params)
    ret_decl = "void" if rt == "void" else rt
    ret_keyword = "" if rt == "void" else "return "

    lines = [
        f"/* {fn.name}  {sig}  (category: {fn.category}) */",
        f"static {ret_decl} {fn.name}_native({param_decl})",
        "{",
        "    (void)env;",
        f"    {ret_keyword}{fn.name}({call_args});",
        "}",
    ]
    return "\n".join(lines)


def emit_table(fns: list[HostFn]) -> str:
    lines = [
        "const NativeSymbol g_tessera_natives[] = {",
    ]
    for fn in fns:
        # Signature strings need one layer of parentheses exactly —
        # `wamr_signature` already wraps in `()`, so pass through.
        lines.append(
            f'    {{ "{fn.name}", (void *){fn.name}_native, "{fn.wamr_signature}", NULL }},'
        )
    lines += [
        "};",
        "",
        "const size_t g_tessera_natives_count =",
        "    sizeof(g_tessera_natives) / sizeof(g_tessera_natives[0]);",
    ]
    return "\n".join(lines)


def emit_c(fns: list[HostFn], skipped: list[tuple[HostFn, str]]) -> str:
    header_includes = sorted({f'#include "core_{fn.category}.h"' for fn in fns})

    parts = [
        WARNING,
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        '#include "wasm_export.h"',
        "",
        "/* The LL layer's *_BASE macros (RCC_BASE, AHB1_BASE, …) are",
        " * defined via per-MCU branches in ll_common.h + ll_rcc.h and",
        " * referenced transitively by ll_pwr.h and friends. Projects",
        " * normally reach them via their auto-generated `core.h`, but",
        " * this TU is project-independent so we pull them in directly. */",
        '#include "ll_common.h"',
        '#include "ll_rcc.h"',
        "",
        "/* SDK headers providing the host symbols we wrap. */",
        *header_includes,
        "",
    ]

    if skipped:
        parts.append("/* Skipped at generation time:")
        for fn, reason in skipped:
            parts.append(f" *   - {fn.name}: {reason}")
        parts.append(" */")
        parts.append("")

    parts.append("/* ---- Adapters ---------------------------------------------------------- */")
    parts.append("")
    for fn in fns:
        parts.append(emit_wrapper(fn))
        parts.append("")

    parts.append("/* ---- NativeSymbol table ----------------------------------------------- */")
    parts.append("")
    parts.append(emit_table(fns))
    parts.append("")
    return "\n".join(parts)


def emit_h(fns: list[HostFn]) -> str:
    parts = [
        WARNING,
        "",
        "#ifndef TESSERA_NATIVES_H",
        "#define TESSERA_NATIVES_H",
        "",
        "#include <stddef.h>",
        "",
        '#include "wasm_export.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        f"/* {len(fns)} Tessera-exposed host symbols wired from manifests. */",
        "extern const NativeSymbol g_tessera_natives[];",
        "extern const size_t g_tessera_natives_count;",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif /* TESSERA_NATIVES_H */",
        "",
    ]
    return "\n".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk-docs", type=Path, default=SDK_DOCS)
    ap.add_argument("--out-c", type=Path, default=DEFAULT_OUT_C)
    ap.add_argument("--out-h", type=Path, default=DEFAULT_OUT_H)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    all_fns = load_manifests(args.sdk_docs)
    kept: list[HostFn] = []
    skipped: list[tuple[HostFn, str]] = []
    for fn in all_fns:
        reason = fn.skip_reason()
        if reason:
            skipped.append((fn, reason))
        else:
            kept.append(fn)

    print(f"[gen_tessera_natives] {len(kept)} exposed, {len(skipped)} skipped",
          file=sys.stderr)
    for fn, reason in skipped:
        print(f"  skip {fn.name}: {reason}", file=sys.stderr)

    c_text = emit_c(kept, skipped)
    h_text = emit_h(kept)

    if args.dry_run:
        print("# --- header ---")
        print(h_text)
        print("# --- source ---")
        print(c_text)
        return 0

    args.out_c.parent.mkdir(parents=True, exist_ok=True)
    args.out_c.write_text(c_text)
    args.out_h.write_text(h_text)
    print(f"[gen_tessera_natives] wrote {args.out_c.relative_to(REPO_ROOT)} + "
          f"{args.out_h.relative_to(REPO_ROOT)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
