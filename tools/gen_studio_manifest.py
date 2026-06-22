#!/usr/bin/env python3
"""
Generate Studio manifests from cores + tiles headers.

Reads C headers, finds doxygen blocks containing `@studio expose ...`, and
emits JSON manifests consumed by the Studio frontend palette and build
service. Produces one merged core manifest and one manifest per tile driver.

`@studio` tag syntax (inside a doxygen block):

    @studio expose category=<str> icon=<glyph> name=<dsl_name> [availability=Core.X,Core.Y]

Param syntax (each `@param` line):

    @param <cname> [{dsl_type}] [[min..max]] [unit] <description>

Example:

    @studio expose category=led icon=☀ name=heartbeat
    @param period_ms [0..60000] ms Time between LED toggles.

Usage:
  tools/gen_studio_manifest.py          # write manifests
  tools/gen_studio_manifest.py --check  # verify manifests match headers
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE_OUT_DIR = ROOT / "manifests"
TILE_OUT_DIR = ROOT / "manifests"
SDK_DOCS_OUT_DIR = ROOT / "manifests" / "sdk-docs"

# Where hal_/ll_ headers live, keyed by layer.
LAYER_DIRS = {"hal": ROOT / "sdk/hal", "ll": ROOT / "sdk/ll"}

# Per-category lower-layer docs. Maps a docs category to the hal_*/ll_*
# headers that implement it, so the SDK reference can show the same surface at
# the Core / HAL / LL layer the reader works in. Each layer value is
# (header_filename, only): `only=None` includes every documented function in
# the header (source order); a list curates + orders the output — required for
# sprawling headers like ll_rcc.h. Categories absent here are Core-only. An
# entry may introduce a category that has no core header at all (e.g. clocks,
# which is config-driven) — give it a label/icon via the `meta` key.
LAYER_HEADERS = {
    "pad": {
        "hal": ("hal_gpio.h", None),
        "ll": ("ll_gpio.h", None),
    },
    "led": {
        "ll": (
            "ll_gpio.h",
            ["ll_gpio_config_output", "ll_gpio_set", "ll_gpio_clear",
             "ll_gpio_toggle", "ll_gpio_read"],
        ),
    },
    "timing": {
        "hal": ("hal_common.h", ["hal_tick", "hal_timeout_expired"]),
        "ll": ("ll_systick.h", ["ll_systick_init", "ll_delay_ms", "ll_delay_us"]),
    },
    "watchdog": {
        "ll": (
            "ll_iwdg.h",
            ["ll_iwdg_init", "ll_iwdg_init_1s", "ll_iwdg_init_2s",
             "ll_iwdg_init_5s", "ll_iwdg_init_10s", "ll_iwdg_refresh",
             "ll_iwdg_caused_reset", "ll_rcc_clear_reset_flags"],
        ),
    },
    "clocks": {
        "meta": {"label": "Clocks", "icon": "⏱"},
        "ll": (
            "ll_rcc.h",
            ["ll_rcc_hsi16_enable", "ll_rcc_hse_enable", "ll_flash_set_latency",
             "ll_flash_latency_for_mhz", "ll_rcc_pll_config", "ll_rcc_pll_enable",
             "ll_rcc_pll_ready", "ll_rcc_set_sysclk", "ll_rcc_wait_sysclk",
             "ll_rcc_set_ahb_div", "ll_rcc_set_apb1_div", "ll_rcc_set_apb2_div"],
        ),
    },
    "serial": {
        "hal": ("hal_uart.h", None),
        "ll": ("ll_uart.h", None),
    },
    "i2c": {
        "hal": ("hal_i2c.h", None),
        "ll": (
            "ll_i2c.h",
            ["ll_i2c_init", "ll_i2c_init_fmp", "ll_i2c_write", "ll_i2c_read",
             "ll_i2c_timing_100k", "ll_i2c_timing_400k", "ll_i2c_timing_1m"],
        ),
    },
    "spi": {
        "hal": ("hal_spi.h", None),
        "ll": ("ll_spi.h", None),
    },
    "timer": {
        "hal": ("hal_timer.h", None),
        "ll": ("ll_tim.h", None),
    },
    "adc": {
        "hal": ("hal_adc.h", None),
        "ll": ("ll_adc.h", None),
    },
    "dac": {
        "hal": ("hal_dac.h", None),
        "ll": ("ll_dac.h", None),
    },
    "power": {
        "ll": (
            "ll_pwr.h",
            ["ll_pwr_enable_backup_access", "ll_pwr_sleep_wfi", "ll_pwr_stop",
             "ll_pwr_standby", "ll_pwr_woke_from_standby", "ll_pwr_clear_standby_flag"],
        ),
    },
    "rng": {
        "ll": ("ll_rng.h", None),
    },
    "usb": {
        # Curated (not None): a documented typedef in the header otherwise
        # parses as a stray "void" entry.
        "hal": (
            "hal_usb_cdc.h",
            ["hal_usb_cdc_init", "hal_usb_cdc_connected", "hal_usb_cdc_write",
             "hal_usb_cdc_printf", "hal_usb_cdc_set_rx_callback", "hal_usb_cdc_rx_ready",
             "hal_usb_cdc_getc", "hal_usb_cdc_rx_try", "hal_usb_cdc_read",
             "hal_usb_cdc_available", "hal_usb_cdc_poll", "hal_usb_hid_send_report"],
        ),
    },
    "rtc": {
        "ll": (
            "ll_rtc.h",
            ["ll_rtc_init", "ll_rtc_set_time", "ll_rtc_get_time", "ll_rtc_set_date",
             "ll_rtc_get_date", "ll_rtc_wakeup_config", "ll_rtc_wakeup_disable",
             "ll_rtc_alarm_a_set", "ll_rtc_alarm_a_disable", "ll_rtc_alarm_a_flag",
             "ll_rtc_alarm_a_clear_flag", "ll_rtc_bkp_read", "ll_rtc_bkp_write"],
        ),
    },
}

C_TO_DSL = {
    "int": "int",
    "int8_t": "int", "int16_t": "int", "int32_t": "int",
    "uint8_t": "int", "uint16_t": "int", "uint32_t": "int",
    "float": "float", "double": "float",
    "bool": "bool", "_Bool": "bool",
}

UNIT_VOCAB = {
    "ns", "us", "ms", "s",
    "hz", "khz", "mhz",
    "v", "mv", "ma", "a", "w",
    "c", "f", "k", "db",
    "rad", "deg", "pct",
    "rpm", "hpa", "pa", "g", "kg",
    "°c", "°f", "%",
}

DOXY_BLOCK_RE = re.compile(r"/\*\*(.*?)\*/", re.DOTALL)
SIG_RE = re.compile(
    r"(?:static\s+inline\s+)?([\w\s\*]+?)\s+(\w+)\s*\(([^;{]*)\)",
)
PARAM_RE = re.compile(
    r"^@param\s+(\S+)\s*(?:\{(\w+)\})?\s*(?:\[(-?[\d.]+)\.\.(-?[\d.]+)\])?\s*(.*)$"
)


def strip_doxy(body):
    out = []
    for line in body.splitlines():
        m = re.match(r"\s*\*\s?(.*)$", line)
        out.append(m.group(1) if m else line.strip())
    return out


def parse_studio_tags(lines):
    """Return every @studio tag in the block as a list of (verb, positional, attrs).

    Brace-delimited bodies (e.g. `enum {KEY=label, ...}`) are extracted
    before the rest of the line is split on whitespace so commas inside
    the braces don't tear the body apart. The parsed body appears in
    `attrs` under the key named by the token preceding the `{`.
    """
    out = []
    for line in lines:
        m = re.match(r"@studio\s+(\w+)\s*(.*)", line.strip())
        if not m:
            continue
        verb = m.group(1)
        rest = m.group(2)

        # Extract a single `<key> {...}` block so its contents (which
        # may include commas) survive the whitespace split below.
        # Only `enum` is recognised today; future brace keys (e.g.,
        # `range { ... }` for explicit value lists) can slot in here
        # without changing the outer tag shape.
        brace_attrs = {}
        brace_match = re.search(r"(\w+)\s*\{([^}]*)\}", rest)
        if brace_match:
            brace_key = brace_match.group(1)
            brace_body = brace_match.group(2)
            if brace_key == "enum":
                brace_attrs["enum"] = parse_enum_body(brace_body)
            else:
                # Unknown brace key — surface via stderr so a typo doesn't
                # silently disappear into the void.
                print(
                    f"warn: @studio {verb}: unknown brace key '{brace_key}' — only 'enum' is recognised",
                    file=sys.stderr,
                )
            rest = (rest[:brace_match.start()] + rest[brace_match.end():]).strip()

        positional = None
        attrs = {}
        for tok in rest.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                attrs[k] = v
            elif positional is None:
                positional = tok
        attrs.update(brace_attrs)
        out.append((verb, positional, attrs))
    return out


def parse_enum_body(body):
    """Parse `K1=label1, K2=label2` into [{c_name, label}, ...].

    Entries with no `=` use the C identifier as its own label. Whitespace
    around keys and labels is stripped. Empty entries (trailing comma,
    etc.) are ignored silently — they're a formatting artifact, not a
    meaningful declaration.
    """
    out = []
    for part in body.split(","):
        part = part.strip()
        if not part:
            continue
        if "=" in part:
            k, v = part.split("=", 1)
            out.append({"c_name": k.strip(), "label": v.strip()})
        else:
            out.append({"c_name": part, "label": part})
    return out


def parse_studio_tag(lines):
    """Back-compat helper — return the first @studio expose tag's attrs."""
    for verb, _pos, attrs in parse_studio_tags(lines):
        if verb == "expose":
            return attrs
    return None


def parse_params(lines):
    out = []
    for raw in lines:
        line = raw.strip()
        m = PARAM_RE.match(line)
        if not m:
            continue
        name, type_override, lo, hi, rest = m.groups()
        entry = {"name": name}
        if type_override:
            entry["type_override"] = type_override
        if lo is not None and hi is not None:
            entry["range"] = [
                float(lo) if "." in lo else int(lo),
                float(hi) if "." in hi else int(hi),
            ]
        rest = rest.strip()
        if rest:
            first, _, tail = rest.partition(" ")
            if first.lower() in UNIT_VOCAB:
                entry["unit"] = first
                rest = tail.strip()
        if rest:
            entry["description"] = rest
        out.append(entry)
    return out


def first_brief(lines):
    parts = []
    capturing = True
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("@"):
            if stripped.startswith("@brief"):
                parts.append(stripped[len("@brief"):].strip())
                capturing = False
                continue
            capturing = False
        elif capturing and stripped:
            parts.append(stripped)
    return " ".join(parts).strip() or None


def extract_signature(source, after_offset):
    # Skip whitespace past the closing `*/` of the doxy block. The next
    # non-whitespace character must be the start of a C declaration — not
    # another comment, not a preprocessor directive. If it is, the doxy
    # block wasn't attached to a declaration (e.g., a file-level header
    # comment sitting above `#ifndef GUARD`) and we bail out. This stops
    # the regex from skipping forward and matching text _inside_ a later
    # doxygen block.
    i = after_offset
    while i < len(source) and source[i].isspace():
        i += 1
    if i >= len(source):
        return None
    head = source[i:i + 2]
    if head.startswith("/") or head.startswith("#"):
        return None
    m = SIG_RE.match(source, i)
    if not m:
        return None
    ret = re.sub(r"\s+", " ", m.group(1)).strip()
    ret = re.sub(r"^(?:static|inline|extern|const)\s+", "", ret)
    ret = re.sub(r"^(?:static|inline|extern|const)\s+", "", ret)
    name = m.group(2)
    raw = m.group(3).strip()
    if raw in ("", "void"):
        params = []
    else:
        params = []
        for p in raw.split(","):
            p = p.strip()
            pm = re.match(r"(.+?)(\w+)\s*$", p)
            if not pm:
                continue
            ptype = re.sub(r"\s+", " ", pm.group(1)).strip()
            pname = pm.group(2).strip()
            params.append({"name": pname, "ctype": ptype})
    return {"returns": ret, "name": name, "params": params}


def dsl_type_of(ctype, override):
    if override:
        return override
    norm = ctype.replace(" *", "*").replace("* ", "*").strip()
    if norm in {"const char*", "char*"}:
        return "string"
    if ctype.strip() in C_TO_DSL:
        return C_TO_DSL[ctype.strip()]
    if norm in C_TO_DSL:
        return C_TO_DSL[norm]
    return f"?{ctype}"


def build_host_entry(tag, doxy_lines, sig, header_name, scope, all_tags=()):
    description = first_brief(doxy_lines)
    doxy_params = parse_params(doxy_lines)

    # Per-parameter studio annotations — currently only `@studio param
    # <cname> enum {...}` carries useful metadata, but this is the seam
    # for future per-param attributes (e.g., bitmasks, unit hints).
    # Indexed by the positional C identifier so we can merge into the
    # dsl_params loop below without altering doxy_params shape.
    studio_params = {}
    for verb, positional, attrs in all_tags:
        if verb == "param" and positional:
            studio_params[positional] = attrs

    # `@studio out_buffer <cname> type=...` identifies which C parameter
    # is an output buffer the driver writes into. Two flavours:
    #
    #   Fixed-length:   length=<N>             → DSL sees `int[N]` return
    #   Cap (variable): cap_param=<other_cname> → DSL sees `int[]` writable
    #                                              param + scalar return for
    #                                              the actual count
    #
    # Fixed-length collapses the buffer param into a return value entirely
    # (the function's C return type is void; DSL sees an `int[N]` return).
    # Cap mode keeps the function returning a scalar count and exposes the
    # buffer as a writable array param the DSL caller hands in.
    out_buffers = {}
    out_buffer_caps = set()  # cnames of cap params (stripped from DSL)
    for verb, positional, attrs in all_tags:
        if verb != "out_buffer" or not positional:
            continue
        if "type" not in attrs:
            print(
                f"warn: {sig['name']}: @studio out_buffer {positional} missing type=",
                file=sys.stderr,
            )
            continue
        has_length = "length" in attrs
        has_cap_param = "cap_param" in attrs
        if not has_length and not has_cap_param:
            print(
                f"warn: {sig['name']}: @studio out_buffer {positional} needs either length=<N> (fixed) or cap_param=<name> (variable)",
                file=sys.stderr,
            )
            continue
        if has_length and has_cap_param:
            print(
                f"warn: {sig['name']}: @studio out_buffer {positional} can't carry both length= and cap_param=",
                file=sys.stderr,
            )
            continue
        entry = {"type": attrs["type"]}
        if has_length:
            try:
                entry["length"] = int(attrs["length"])
            except ValueError:
                print(
                    f"warn: {sig['name']}: @studio out_buffer {positional} length={attrs['length']!r} must be an integer",
                    file=sys.stderr,
                )
                continue
        else:
            entry["cap_param"] = attrs["cap_param"]
            out_buffer_caps.add(attrs["cap_param"])
        out_buffers[positional] = entry

    # `@studio out_scalar <cname> type=<ctype>` identifies a scalar pointer
    # parameter (`Type *<cname>`) that the driver writes into. The DSL caller
    # passes a *local* (lvalue Ident) into the slot and the value is back-
    # filled when the call returns. Multiple out_scalars per host are allowed
    # (this is how multi-out functions like `self_test(*accel, *gyro)` get
    # exposed faithfully). The param stays in the DSL-visible signature and
    # the entry carries `out_scalar: True` so consumers (type checker /
    # codegen) can enforce the lvalue rule and emit `&` (C) or stage memory
    # (Wasm) at the call site.
    out_scalars = {}
    for verb, positional, attrs in all_tags:
        if verb != "out_scalar" or not positional:
            continue
        if "type" not in attrs:
            print(
                f"warn: {sig['name']}: @studio out_scalar {positional} missing type=",
                file=sys.stderr,
            )
            continue
        out_scalars[positional] = {"type": attrs["type"]}

    # `@studio in_buffer <cname> type=<element> length_param=<other_cname>
    #     [length=<N>]`
    # identifies which C parameter is a caller-passed array buffer + which
    # adjacent param carries its length. Both flavours strip the buffer
    # param + length param from the DSL-facing list and emit a single
    # array DSL param in the buffer's position:
    #
    #   Fixed-length (length=<N>):    DSL sees `int[N]`. Codegen splices
    #                                 the literal N at the call site
    #                                 regardless of caller's array.
    #   Variable (no length=):        DSL sees `int[]`. Codegen reads the
    #                                 caller's array length at the call
    #                                 site and splices that into the C
    #                                 count slot.
    in_buffers = {}      # cname → { type, length?, length_param? }
    in_buffer_lengths = set()  # cnames of length params (stripped from DSL)
    for verb, positional, attrs in all_tags:
        if verb != "in_buffer" or not positional:
            continue
        if "type" not in attrs:
            print(
                f"warn: {sig['name']}: @studio in_buffer {positional} missing type=",
                file=sys.stderr,
            )
            continue
        has_length = "length" in attrs
        has_length_param = "length_param" in attrs
        # Three valid shapes:
        #   length=N + length_param=<name>  → fixed-length, C count arg present
        #   length_param=<name> only         → variable-length, count from caller
        #   length=N only                    → fixed-length, C function has no count arg
        if not has_length and not has_length_param:
            print(
                f"warn: {sig['name']}: @studio in_buffer {positional} needs length= (fixed without count arg), length_param= (variable), or both (fixed with count arg)",
                file=sys.stderr,
            )
            continue
        entry = {"type": attrs["type"]}
        if has_length_param:
            entry["length_param"] = attrs["length_param"]
        if has_length:
            try:
                entry["length"] = int(attrs["length"])
            except ValueError:
                print(
                    f"warn: {sig['name']}: @studio in_buffer {positional} length={attrs['length']!r} must be an integer",
                    file=sys.stderr,
                )
                continue
        in_buffers[positional] = entry
        if has_length_param:
            in_buffer_lengths.add(attrs["length_param"])

    receiver = None
    c_params = []
    for sp in sig["params"]:
        norm = sp["ctype"].replace(" *", "*").replace("* ", "*").strip()
        if norm == "tile_t*":
            receiver = sp["ctype"]
        elif sp["name"] in out_buffers:
            # Fixed-length out-buffer collapses entirely into a return
            # value; cap-mode out-buffer surfaces as a writable DSL array
            # param (handled in the dsl_params loop further down).
            ob = out_buffers[sp["name"]]
            if "length" in ob:
                continue
            else:
                c_params.append(sp)
        elif sp["name"] in in_buffer_lengths:
            # Length params paired with an `@studio in_buffer` are
            # implicit at the DSL layer (array.length supplies them).
            continue
        elif sp["name"] in out_buffer_caps:
            # Cap params paired with a cap-mode `@studio out_buffer`
            # are implicit (the writable array's declared length supplies
            # them).
            continue
        else:
            c_params.append(sp)

    # Warn when out_buffer / in_buffer annotations don't line up with
    # the C signature — catches typos (`@studio out_buffer buf` when
    # the param is named `buffer`).
    c_param_names = {sp["name"] for sp in sig["params"]}
    for cname in out_buffers:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @studio out_buffer {cname} doesn't match any C parameter",
                file=sys.stderr,
            )
    for cname in in_buffers:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @studio in_buffer {cname} doesn't match any C parameter",
                file=sys.stderr,
            )
    for cname in out_scalars:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @studio out_scalar {cname} doesn't match any C parameter",
                file=sys.stderr,
            )
    for cname in in_buffer_lengths:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @studio in_buffer length_param={cname} doesn't match any C parameter",
                file=sys.stderr,
            )

    # Cap-mode out_buffers stay in `c_params` (they're DSL-visible as
    # writable arrays); fixed-length out_buffers were stripped above.
    fixed_out_buffer_count = sum(1 for ob in out_buffers.values() if "length" in ob)
    expected_doxy = len(c_params) + fixed_out_buffer_count + len(in_buffer_lengths) + len(out_buffer_caps)
    if len(doxy_params) != expected_doxy:
        print(
            f"warn: {sig['name']}: @param count {len(doxy_params)} != C arg count "
            f"{len(c_params)} + fixed out_buffer {fixed_out_buffer_count} "
            f"+ in_buffer length params {len(in_buffer_lengths)} "
            f"+ out_buffer cap params {len(out_buffer_caps)}",
            file=sys.stderr,
        )

    for cname in out_buffer_caps:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @studio out_buffer cap_param={cname} doesn't match any C parameter",
                file=sys.stderr,
            )

    # Index doxy params by their declared C name so we survive a
    # stripped out_buffer in the middle of the list (positional index
    # would misalign). Fall back to positional alignment for params
    # that don't match by name (e.g., author renamed the DSL param in
    # the @param line without touching the C signature).
    doxy_by_name = {p["name"]: p for p in doxy_params}
    dsl_params = []
    for i, cp in enumerate(c_params):
        meta = doxy_by_name.get(cp["name"])
        if meta is None:
            meta = doxy_params[i] if i < len(doxy_params) else {"name": cp["name"]}
        # Array IN / cap-mode OUT params get a DSL array type from the
        # @studio in_buffer / @studio out_buffer annotation,
        # overriding the underlying C pointer type. Fixed-length renders
        # as `int[N]`; variable / cap-mode renders as `int[]`.
        if cp["name"] in in_buffers:
            ib = in_buffers[cp["name"]]
            element_dsl = dsl_type_of(ib["type"], None)
            if "length" in ib:
                entry_type = f"{element_dsl}[{ib['length']}]"
            else:
                entry_type = f"{element_dsl}[]"
        elif cp["name"] in out_buffers and "length" not in out_buffers[cp["name"]]:
            ob = out_buffers[cp["name"]]
            element_dsl = dsl_type_of(ob["type"], None)
            entry_type = f"{element_dsl}[]"
        elif cp["name"] in out_scalars:
            os_ = out_scalars[cp["name"]]
            entry_type = dsl_type_of(os_["type"], None)
        else:
            entry_type = dsl_type_of(cp["ctype"], meta.get("type_override"))
        entry = {
            "name": meta["name"],
            "type": entry_type,
        }
        if cp["name"] in out_scalars:
            entry["out_scalar"] = True
        if "range" in meta:
            entry["range"] = meta["range"]
        if "unit" in meta:
            entry["unit"] = meta["unit"]
        if "description" in meta:
            entry["description"] = meta["description"]
        # Attach per-param studio annotations when present. Matched by
        # either the C identifier (`cp["name"]`) or the DSL-facing name
        # — the latter catches the common pattern where @param renames
        # a parameter for DSL friendliness.
        #
        #   - `type=<dsl_type>` overrides the C-inferred DSL type. Used
        #     for array-in-param annotations like `type=int[16]` where
        #     the C type is a pointer but the DSL sees a fixed-length
        #     array.
        #   - `enum {K=label, ...}` attaches friendly labels for
        #     int-valued enum C params.
        tp = studio_params.get(cp["name"]) or studio_params.get(entry["name"])
        if tp:
            if "type" in tp:
                entry["type"] = tp["type"]
            if "enum" in tp:
                entry["enum"] = tp["enum"]
        dsl_params.append(entry)

    category = tag.get("category", "?")
    dsl_name = tag.get("name", sig["name"])
    qname = ["$slot", dsl_name] if scope == "tile" else [category, dsl_name]

    host = {
        "qname": qname,
        "symbol": sig["name"],
        "header": header_name,
        "category": category,
        "description": description,
        "params": dsl_params,
        "returns": sig["returns"],
    }
    # DSL-visible return type — explicitly declared on the @studio expose
    # tag via `returns=<int|bool|float|string>`. Drives the DSL's import
    # return-type clause (`import X.Y() -> int`) and makes the host
    # callable as a CallExpr. Void hosts (no `returns=`) stay statement-
    # only; this is orthogonal to the C-level `returns` field above which
    # records the underlying C return type verbatim.
    if "returns" in tag:
        host["dsl_returns"] = tag["returns"]
    if "icon" in tag:
        host["icon"] = tag["icon"]
    # Array-returning hosts: the frontend needs the element type +
    # length to declare the right stack buffer before the call. We
    # only support one out-buffer per host (matching the manifest
    # schema on the consumer side). Multiple annotations collapse
    # to the first with a warning.
    if out_buffers:
        if len(out_buffers) > 1:
            print(
                f"warn: {sig['name']}: multiple @studio out_buffer annotations — only one out-buffer per host is supported (using the first)",
                file=sys.stderr,
            )
        first_name = next(iter(out_buffers))
        ob = out_buffers[first_name]
        # Cap-mode out_buffers are DSL-visible writable params; the
        # frontend resolver needs to know the param's name. Fixed-length
        # out_buffers collapse into the host's return value, so the
        # name is irrelevant there.
        if "cap_param" in ob:
            host["c_out_buffer"] = {"name": first_name, **ob}
        else:
            host["c_out_buffer"] = ob
    if out_scalars:
        # Preserve C-signature order (dict iteration follows insertion =
        # parse order, but the parse loop visits @studio tags in source
        # order — sort by C param position so codegen emits args in the
        # right slot).
        order = {sp["name"]: i for i, sp in enumerate(sig["params"])}
        host["c_out_scalars"] = [
            {"name": n, **out_scalars[n]}
            for n in sorted(out_scalars.keys(), key=lambda n: order.get(n, 1 << 30))
        ]
    if in_buffers:
        if len(in_buffers) > 1:
            print(
                f"warn: {sig['name']}: multiple @studio in_buffer annotations — only one in-buffer per host is supported (using the first)",
                file=sys.stderr,
            )
        first_name = next(iter(in_buffers))
        # `name` is the DSL-facing param name, which equals the C param
        # name for any in_buffer flow that survived the doxy reconciliation.
        # `length_param` carries the C name of the count slot so codegen
        # can splice the literal length value into the C call in the right
        # position. `length` is the fixed array length (the array's `[N]`).
        host["c_in_buffer"] = {
            "name": first_name,
            **in_buffers[first_name],
        }
    if receiver:
        host["receiver"] = receiver.strip()
    if "availability" in tag:
        host["availability"] = {"cores": tag["availability"].split(",")}
    return host


def parse_header(path, scope):
    """Return (hosts, sections, docs, events).

    `hosts` — palette-facing entries for functions tagged `@studio expose`.
    `sections` — file-scope metadata from `@studio category`/`@studio tile`.
    `docs` — docs-facing entries for every documented function in the file,
             whether or not it's Studio-exposed. This is what feeds the
             SDK reference pages on the website.
    `events` — event declarations from `@studio event name=<id>
               [description="..."] [payload=n:t,n:t,...]`. Valid in both
               scopes; tile events carry extra `mask`/`read`/`read_type`
               attributes tied to the tile driver's on_event ABI, core
               events rely on coregen emitting a subsystem-specific
               dispatcher (see core_pad.h → pad-edge dispatcher).

    Core-scope returns `sections = {"<category>": {label, icon}, ...}`.
    Tile-scope returns `sections = {"<tile>": {label, icon}}` with a single
    entry keyed by the tile palette label.
    """
    source = path.read_text()
    hosts = []
    sections = {}
    docs = []
    events = []
    for m in DOXY_BLOCK_RE.finditer(source):
        lines = strip_doxy(m.group(1))
        tags = parse_studio_tags(lines)

        for verb, positional, attrs in tags:
            if verb == "category" and scope == "core" and positional:
                sections[positional] = {
                    "label": attrs.get("label", positional),
                    "icon": attrs.get("icon", ""),
                }
            elif verb == "tile" and scope == "tile":
                label = attrs.get("label", path.stem)
                sections[label] = {
                    "label": label,
                    "icon": attrs.get("icon", ""),
                }
            elif verb == "event":
                name = attrs.get("name")
                if not name:
                    print(
                        f"warn: {path.name}: @studio event missing name=",
                        file=sys.stderr,
                    )
                    continue
                entry = {
                    "name": name,
                    "payload": parse_event_payload(attrs.get("payload", "")),
                }
                if "description" in attrs:
                    entry["description"] = attrs["description"]
                if "icon" in attrs:
                    entry["icon"] = attrs["icon"]
                if "mask" in attrs:
                    # C expression the dispatcher AND's against the `events`
                    # bitmask to detect this event firing. Typically a #define
                    # from the tile driver header (e.g., ICM42686P_INT_TILT_DET).
                    entry["mask"] = attrs["mask"]
                if "read" in attrs:
                    # Driver function that populates a payload struct. The
                    # dispatcher calls it when the event fires and passes
                    # the struct fields as handler arguments by name.
                    # Signature is `void read(tile_t *, <read_type> *)`.
                    entry["read"] = attrs["read"]
                if "read_type" in attrs:
                    # C struct type whose fields match the event's payload
                    # parameter names. Declared as a stack local inside the
                    # dispatch branch before calling `read`.
                    entry["read_type"] = attrs["read_type"]
                events.append(entry)

        sig = extract_signature(source, m.end())
        if not sig:
            # Floating doxygen block with no following function — fine,
            # probably a file-level comment. Skip silently.
            continue

        expose = next((a for v, _, a in tags if v == "expose"), None)
        if expose:
            hosts.append(build_host_entry(expose, lines, sig, path.name, scope, tags))

        # Docs entry: collect every function with a preceding doxygen block
        # so the SDK reference page includes init / sos / etc. even though
        # they're not palette-exposed.
        docs.append(build_doc_entry(lines, sig, bool(expose), source, m.end()))
    return hosts, sections, docs, events


def parse_event_payload(spec):
    """Split a `payload=name:type,name:type,...` attribute into a list of
    {name, type} dicts. Empty spec returns []; types default to int."""
    if not spec:
        return []
    out = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if ":" in part:
            name, typ = part.split(":", 1)
            out.append({"name": name.strip(), "type": typ.strip()})
        else:
            out.append({"name": part, "type": "int"})
    return out


def build_doc_entry(doxy_lines, sig, studio_exposed, source, doxy_end_offset):
    """Build a docs-facing entry for the SDK reference pages.

    Carries richer C-level detail than the palette `host` entry: C parameter
    types (not DSL-mapped), attributes like `noreturn` pulled off the
    declaration line, and the signature as a single string the website can
    format. Params without `@param` lines still appear — their description
    is empty — so hidden-but-documented functions (e.g., no-arg init) land
    cleanly on the page.
    """
    description = first_brief(doxy_lines)
    doxy_params = parse_params(doxy_lines)
    by_name = {p["name"]: p for p in doxy_params}

    doc_params = []
    for i, cp in enumerate(sig["params"]):
        # Prefer name-matched doxy meta; fall back to positional; fall back
        # to the C param name when no doxy exists at all (e.g., a void-init).
        meta = by_name.get(cp["name"]) or (
            doxy_params[i] if i < len(doxy_params) else {"name": cp["name"]}
        )
        entry = {
            "name": meta.get("name", cp["name"]),
            "ctype": cp["ctype"],
        }
        if "range" in meta:
            entry["range"] = meta["range"]
        if "unit" in meta:
            entry["unit"] = meta["unit"]
        if "description" in meta:
            entry["description"] = meta["description"]
        doc_params.append(entry)

    signature = f"{sig['name']}(" + format_c_params(sig["params"]) + ")"
    attributes = detect_attributes(source, doxy_end_offset, sig["name"])

    entry = {
        "name": sig["name"],
        "signature": signature,
        "returns": sig["returns"],
        "brief": description,
        "params": doc_params,
        "studio_exposed": studio_exposed,
    }
    if attributes:
        entry["attributes"] = attributes
    return entry


def parse_layer_docs(path, layer, only=None):
    """Docs-facing entries for the hal_*/ll_* functions in `path`, tagged `layer`.

    Reuses the core docs path (Doxygen block -> build_doc_entry) but these are
    never Studio-exposed. Duplicate names — the family-gated `#if` variants in
    e.g. ll_rcc.h — collapse to the first occurrence. `only` (a list of names)
    curates and orders the output and is required for sprawling headers;
    without it, every documented function in the header is included in source
    order. Names in `only` that aren't found (or aren't Doxygen'd) are warned
    about, not silently dropped.
    """
    source = path.read_text()
    by_name = {}
    order = []
    for m in DOXY_BLOCK_RE.finditer(source):
        sig = extract_signature(source, m.end())
        if not sig or sig["name"] in by_name:
            continue
        entry = build_doc_entry(strip_doxy(m.group(1)), sig, False, source, m.end())
        entry["layer"] = layer
        by_name[sig["name"]] = entry
        order.append(sig["name"])
    if only is None:
        return [by_name[n] for n in order]
    missing = [n for n in only if n not in by_name]
    if missing:
        print(
            f"warn: {path.name}: layer-doc functions not found or undocumented: "
            f"{', '.join(missing)}",
            file=sys.stderr,
        )
    return [by_name[n] for n in only if n in by_name]


def format_c_params(params):
    if not params:
        return "void"
    return ", ".join(f"{p['ctype']} {p['name']}" for p in params)


def detect_attributes(source, after_offset, _fn_name):
    """Pick up `__attribute__((...))` annotations on the declaration that
    immediately follows the doxy block. Scans only up to the next `;` or
    `{` — that's the end of this declaration. A wider window would leak
    attributes from later functions (e.g., core_led_blink catching the
    noreturn off a later forward-declared core_led_sos)."""
    end = len(source)
    for ch in (";", "{"):
        idx = source.find(ch, after_offset)
        if idx != -1 and idx < end:
            end = idx
    window = source[after_offset:end]
    attrs = []
    for match in re.finditer(r"__attribute__\s*\(\s*\(\s*(\w+)\s*\)\s*\)", window):
        name = match.group(1)
        if name == "noreturn" and "noreturn" not in attrs:
            attrs.append("noreturn")
    return attrs


def source_commit():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=ROOT, stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        return "unknown"


def serialize(data):
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def load_bus_addresses(def_path):
    """Extract per-bus address variants from a tile-definition JSON.

    Returns a dict keyed by bus name (upper-cased, e.g. "I2C"), each
    value a list of `{address, is_default}` entries pulled verbatim
    from `interfaces[name=<bus>].parameters.addresses`. Buses without
    an `addresses` list (I3C's dynamic assignment, SPI's CS-based
    selection) are omitted. Empty dict when the file is missing, the
    JSON has no interfaces, or no interface carries addresses — the
    frontend treats that as "one fixed address, no user choice".

    No schema change to the tile-def format; the data is already
    present for every tile with `parameters.addresses`. This helper
    just surfaces it onto the tile manifest so Studio can cap bus
    capacity and render an address selector per-row.
    """
    if def_path is None:
        return {}
    try:
        raw = json.loads(Path(def_path).read_text())
    except FileNotFoundError:
        print(f"warn: tile definition not found: {def_path}", file=sys.stderr)
        return {}
    except json.JSONDecodeError as err:
        print(f"warn: tile definition {def_path} is invalid JSON: {err}", file=sys.stderr)
        return {}

    out = {}
    for iface in raw.get("interfaces", []) or []:
        name = iface.get("name")
        if not isinstance(name, str):
            continue
        addrs = iface.get("parameters", {}).get("addresses", []) or []
        if not addrs:
            continue
        # Copy each entry to strip any extra fields the tile-def schema
        # may add later (keeps the manifest shape stable).
        out[name] = [
            {
                "address": a["address"],
                **({"is_default": True} if a.get("is_default") else {}),
            }
            for a in addrs
            if isinstance(a, dict) and "address" in a
        ]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if manifests on disk are out of sync")
    args = ap.parse_args()

    commit = source_commit()

    # Auto-discover every `sdk/core/core_*.h` header; anything without a
    # `@studio category` tag contributes nothing, so new modules opt in
    # simply by adding the tag. No hand-maintained source list.
    core_sources = sorted((ROOT / "sdk/core").glob("core_*.h"))
    core_hosts = []
    core_events = []
    core_categories = {}
    # Per-category docs — functions documented in each tagged header,
    # keyed by the category's canonical name (led, usb, adc, ...). The
    # website SDK pages consume these JSON files directly.
    sdk_docs = {}
    for p in core_sources:
        hosts, sections, docs, events = parse_header(p, scope="core")
        core_hosts.extend(hosts)
        # Attach the surrounding category to each event so Studio's DSL
        # codegen + block palette can group them under the right header.
        # Core events declared outside a `@studio category` block are
        # skipped with a warning — the palette needs somewhere to show them.
        if events:
            if len(sections) == 1:
                cat = next(iter(sections.keys()))
                for e in events:
                    e["category"] = cat
                core_events.extend(events)
            else:
                print(
                    f"warn: {p.name}: {len(events)} @studio event(s) but "
                    f"{len(sections)} @studio category declarations — "
                    f"events dropped (need exactly one category per file)",
                    file=sys.stderr,
                )
        for name, meta in sections.items():
            # First declaration wins — later duplicates are ignored so two
            # files claiming the same category don't silently clobber each
            # other. Logged for visibility.
            if name in core_categories and core_categories[name] != meta:
                print(f"warn: category '{name}' redeclared in {p.name}", file=sys.stderr)
                continue
            core_categories[name] = meta
            for fn in docs:
                fn["layer"] = "core"
            sdk_docs[name] = {
                "schema": "studio-sdk-docs/v2",
                "source": f"tiles@{commit}",
                "category": name,
                "label": meta["label"],
                "icon": meta["icon"],
                "header": p.name,
                "headers": {"core": p.name},
                "functions": docs,
            }

    # Augment categories with their HAL / LL surface so the SDK reference can
    # render at whichever layer the reader works in. A category may be brand
    # new here (no core header — e.g. clocks, which is config-driven). See
    # LAYER_HEADERS.
    for category, spec in LAYER_HEADERS.items():
        doc = sdk_docs.get(category)
        if doc is None:
            meta = spec.get("meta", {})
            doc = {
                "schema": "studio-sdk-docs/v2",
                "source": f"tiles@{commit}",
                "category": category,
                "label": meta.get("label", category),
                "icon": meta.get("icon", ""),
                "header": None,
                "headers": {},
                "functions": [],
            }
            sdk_docs[category] = doc
        for layer in ("hal", "ll"):
            if layer not in spec:
                continue
            fname, only = spec[layer]
            doc["functions"].extend(parse_layer_docs(LAYER_DIRS[layer] / fname, layer, only))
            doc["headers"][layer] = fname

    core_manifest = {
        "schema": "studio-manifest/v1",
        "source": f"tiles@{commit}",
        "categories": core_categories,
        "hosts": core_hosts,
        "events": core_events,
    }

    tile_sources = [
        {
            "path": ROOT / "drivers/tile_display_rgbw.h",
            "definition": ROOT / "definitions/Display-RGBW-a.json",
            "prefix": "tile_display_rgbw",
            "init": "tile_display_rgbw_init",
            "version": "2.2.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_i_6p6.h",
            "definition": ROOT / "definitions/Sense-I-6P6-a.json",
            "prefix": "tile_sense_i_6p6",
            "init": "tile_sense_i_6p6_init",
            "version": "1.2.0",
        },
        {
            "path": ROOT / "drivers/tile_drive_h.h",
            "definition": ROOT / "definitions/Drive-H-a.json",
            "prefix": "tile_drive_h",
            "init": "tile_drive_h_init",
            "version": "4.1.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_mic.h",
            "definition": ROOT / "definitions/Sense-MIC-a.json",
            "prefix": "tile_sense_mic",
            "init": "tile_sense_mic_init",
            "version": "2.1.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_i_9.h",
            "definition": ROOT / "definitions/Sense-I-9-c.json",
            "prefix": "tile_sense_i_9",
            "init": "tile_sense_i_9_init",
            "version": "3.1.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_t_c.h",
            "definition": ROOT / "definitions/Sense-T-C-a.json",
            "prefix": "tile_sense_t_c",
            "init": "tile_sense_t_c_init",
            "version": "1.3.0",
        },
        {
            "path": ROOT / "drivers/tile_drive_a_2.h",
            "definition": ROOT / "definitions/Drive-A-2-a.json",
            "prefix": "tile_drive_a_2",
            "init": "tile_drive_a_2_init",
            "version": "3.1.0",
        },
        {
            "path": ROOT / "drivers/tile_drive_p.h",
            "definition": ROOT / "definitions/Drive-P-a.json",
            "prefix": "tile_drive_p",
            "init": "tile_drive_p_init",
            "version": "3.1.0",
        },
        {
            "path": ROOT / "drivers/tile_power_l_1t.h",
            "definition": ROOT / "definitions/Power-L-1T-b.json",
            "prefix": "tile_power_l_1t",
            "init": "tile_power_l_1t_init",
            "version": "3.2.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_bp.h",
            "definition": ROOT / "definitions/Sense-BP-a.json",
            "prefix": "tile_sense_bp",
            "init": "tile_sense_bp_init",
            "version": "1.2.0",
        },
        {
            "path": ROOT / "drivers/tile_sense_tof.h",
            "definition": ROOT / "definitions/Sense-TOF-a.json",
            "prefix": "tile_sense_tof",
            "init": "tile_sense_tof_init",
            "version": "1.3.0",
        },
        {
            "path": ROOT / "drivers/tile_drive_dc_h.h",
            "definition": ROOT / "definitions/Drive-DC-H-a.json",
            "prefix": "tile_drive_dc_h",
            "init": "tile_drive_dc_h_init",
            "version": "4.1.0",
        },
        {
            "path": ROOT / "drivers/tile_store_o_128.h",
            "definition": ROOT / "definitions/Store-O-128-a.json",
            "prefix": "tile_store_o_128",
            "init": "tile_store_o_128_init",
            "version": "1.0.0",
        },
    ]

    targets = [(CORE_OUT_DIR / "core.json", core_manifest)]

    # Per-category SDK docs JSONs — one file per `@studio category`.
    for category, doc in sdk_docs.items():
        targets.append((SDK_DOCS_OUT_DIR / f"{category}.json", doc))

    for t in tile_sources:
        hosts, sections, _docs, events = parse_header(t["path"], scope="tile")
        if len(sections) != 1:
            print(
                f"warn: {t['path'].name}: expected exactly one @studio tile tag, "
                f"found {len(sections)}",
                file=sys.stderr,
            )
        palette = next(iter(sections.values())) if sections else {
            "label": t["path"].stem, "icon": "",
        }
        manifest = {
            "schema": "studio-manifest/v1",
            "source": f"tiles@{commit}",
            "tile": palette["label"],
            "palette": palette,
            "driver": {
                "prefix": t["prefix"],
                "header": t["path"].name,
                "version": t["version"],
            },
            "handle": {"type": "tile_t", "init": t["init"]},
            "hosts": hosts,
            "events": events,
        }
        # Carry per-bus address variants from the tile-def JSON so
        # the frontend can cap how many instances of this tile fit on a
        # bus (I2C: one per address) and surface a variant selector when
        # the tile supports more than one. Absent/empty when the tile
        # has no definition file or none of its interfaces declare
        # addresses — the frontend treats that as "one fixed address,
        # no user choice".
        bus_addrs = load_bus_addresses(t.get("definition"))
        if bus_addrs:
            manifest["bus_addresses"] = bus_addrs
        targets.append((TILE_OUT_DIR / f"{t['path'].stem}.json", manifest))

    if args.check:
        # The `source` sha tracks the commit, not the content: it legitimately
        # differs from HEAD between a regen and the commit that lands it (and
        # always differs in CI, which runs on a later commit). Normalise it on
        # both sides so --check flags only *structural* drift — a header edited
        # without regenerating its manifest.
        def strip_source(s):
            return re.sub(r'"source": "tiles@[^"]*"', '"source": "tiles@<sha>"', s)

        drift = []
        for path, data in targets:
            want = strip_source(serialize(data))
            have = strip_source(path.read_text()) if path.exists() else ""
            if have != want:
                drift.append(path)
        if drift:
            for p in drift:
                print(f"drift: {p}", file=sys.stderr)
            sys.exit(1)
        print("manifests up to date")
        return

    for path, data in targets:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(serialize(data))
        # Palette manifests carry `hosts`; SDK-docs carry `functions`.
        if "hosts" in data:
            summary = f"{len(data['hosts'])} hosts, {len(data.get('events', []))} events"
        else:
            summary = f"{len(data.get('functions', []))} functions"
        print(f"wrote {path.relative_to(ROOT)}  ({summary})")


if __name__ == "__main__":
    main()
