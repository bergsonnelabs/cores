#!/usr/bin/env python3
"""
Generate Tessera manifests from cores + kiln headers.

Reads C headers, finds doxygen blocks containing `@tessera expose ...`, and
emits JSON manifests consumed by the Tessera frontend palette and build
service. Produces one merged core manifest and one manifest per tile driver.

`@tessera` tag syntax (inside a doxygen block):

    @tessera expose category=<str> icon=<glyph> name=<dsl_name> [availability=Core.X,Core.Y]

Param syntax (each `@param` line):

    @param <cname> [{dsl_type}] [[min..max]] [unit] <description>

Example:

    @tessera expose category=led icon=☀ name=heartbeat
    @param period_ms [0..60000] ms Time between LED toggles.

Usage:
  tools/gen_tessera_manifest.py          # write manifests
  tools/gen_tessera_manifest.py --check  # verify manifests match headers
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE_OUT_DIR = ROOT / "manifests"
TILE_OUT_DIR = ROOT / "kiln" / "manifests"
SDK_DOCS_OUT_DIR = ROOT / "manifests" / "sdk-docs"

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


def parse_tessera_tags(lines):
    """Return every @tessera tag in the block as a list of (verb, positional, attrs).

    Brace-delimited bodies (e.g. `enum {KEY=label, ...}`) are extracted
    before the rest of the line is split on whitespace so commas inside
    the braces don't tear the body apart. The parsed body appears in
    `attrs` under the key named by the token preceding the `{`.
    """
    out = []
    for line in lines:
        m = re.match(r"@tessera\s+(\w+)\s*(.*)", line.strip())
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
                    f"warn: @tessera {verb}: unknown brace key '{brace_key}' — only 'enum' is recognised",
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


def parse_tessera_tag(lines):
    """Back-compat helper — return the first @tessera expose tag's attrs."""
    for verb, _pos, attrs in parse_tessera_tags(lines):
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

    # Per-parameter tessera annotations — currently only `@tessera param
    # <cname> enum {...}` carries useful metadata, but this is the seam
    # for future per-param attributes (e.g., bitmasks, unit hints).
    # Indexed by the positional C identifier so we can merge into the
    # dsl_params loop below without altering doxy_params shape.
    tessera_params = {}
    for verb, positional, attrs in all_tags:
        if verb == "param" and positional:
            tessera_params[positional] = attrs

    # `@tessera out_buffer <cname> type=... length=...` identifies which
    # C parameter is actually an output buffer — the driver writes into
    # it and returns void. The DSL wants to see this as an array return,
    # not a pointer-in, so we strip the param from the DSL-visible list
    # and emit `c_out_buffer` metadata for the frontend resolver.
    out_buffers = {}
    for verb, positional, attrs in all_tags:
        if verb != "out_buffer" or not positional:
            continue
        if "type" not in attrs or "length" not in attrs:
            print(
                f"warn: {sig['name']}: @tessera out_buffer {positional} missing type= or length=",
                file=sys.stderr,
            )
            continue
        try:
            length = int(attrs["length"])
        except ValueError:
            print(
                f"warn: {sig['name']}: @tessera out_buffer {positional} length={attrs['length']!r} must be an integer",
                file=sys.stderr,
            )
            continue
        out_buffers[positional] = {"type": attrs["type"], "length": length}

    receiver = None
    c_params = []
    for sp in sig["params"]:
        norm = sp["ctype"].replace(" *", "*").replace("* ", "*").strip()
        if norm == "tile_t*":
            receiver = sp["ctype"]
        elif sp["name"] in out_buffers:
            # Drop the out-buffer param from the DSL-facing list; its
            # presence is carried by host["c_out_buffer"] below.
            continue
        else:
            c_params.append(sp)

    # Warn when out_buffer annotations don't line up with the C
    # signature — catches typos (`@tessera out_buffer buf` when the
    # param is named `buffer`).
    c_param_names = {sp["name"] for sp in sig["params"]}
    for cname in out_buffers:
        if cname not in c_param_names:
            print(
                f"warn: {sig['name']}: @tessera out_buffer {cname} doesn't match any C parameter",
                file=sys.stderr,
            )

    if len(doxy_params) != len(c_params) + len(out_buffers):
        # out_buffer params usually have their own @param line (useful
        # for docs), but they're stripped from the DSL-visible count.
        # We still emit the warning if the totals don't match even
        # accounting for stripped buffers.
        print(
            f"warn: {sig['name']}: @param count {len(doxy_params)} != C arg count {len(c_params)} + out_buffer count {len(out_buffers)}",
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
        entry = {
            "name": meta["name"],
            "type": dsl_type_of(cp["ctype"], meta.get("type_override")),
        }
        if "range" in meta:
            entry["range"] = meta["range"]
        if "unit" in meta:
            entry["unit"] = meta["unit"]
        if "description" in meta:
            entry["description"] = meta["description"]
        # Attach per-param tessera annotations when present. Matched by
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
        tp = tessera_params.get(cp["name"]) or tessera_params.get(entry["name"])
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
    # DSL-visible return type — explicitly declared on the @tessera expose
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
                f"warn: {sig['name']}: multiple @tessera out_buffer annotations — only one out-buffer per host is supported (using the first)",
                file=sys.stderr,
            )
        first_name = next(iter(out_buffers))
        host["c_out_buffer"] = out_buffers[first_name]
    if receiver:
        host["receiver"] = receiver.strip()
    if "availability" in tag:
        host["availability"] = {"cores": tag["availability"].split(",")}
    return host


def parse_header(path, scope):
    """Return (hosts, sections, docs, events).

    `hosts` — palette-facing entries for functions tagged `@tessera expose`.
    `sections` — file-scope metadata from `@tessera category`/`@tessera tile`.
    `docs` — docs-facing entries for every documented function in the file,
             whether or not it's Tessera-exposed. This is what feeds the
             SDK reference pages on the website.
    `events` — event declarations from `@tessera event name=<id>
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
        tags = parse_tessera_tags(lines)

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
                        f"warn: {path.name}: @tessera event missing name=",
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


def build_doc_entry(doxy_lines, sig, tessera_exposed, source, doxy_end_offset):
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
        "tessera_exposed": tessera_exposed,
    }
    if attributes:
        entry["attributes"] = attributes
    return entry


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
    """Extract per-bus address variants from a kiln tile-definition JSON.

    Returns a dict keyed by bus name (upper-cased, e.g. "I2C"), each
    value a list of `{address, is_default}` entries pulled verbatim
    from `interfaces[name=<bus>].parameters.addresses`. Buses without
    an `addresses` list (I3C's dynamic assignment, SPI's CS-based
    selection) are omitted. Empty dict when the file is missing, the
    JSON has no interfaces, or no interface carries addresses — the
    frontend treats that as "one fixed address, no user choice".

    No schema change to the tile-def format; the data is already
    present for every tile with `parameters.addresses`. This helper
    just surfaces it onto the tile manifest so Tessera can cap bus
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
    # `@tessera category` tag contributes nothing, so new modules opt in
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
        # Attach the surrounding category to each event so Tessera's DSL
        # codegen + block palette can group them under the right header.
        # Core events declared outside a `@tessera category` block are
        # skipped with a warning — the palette needs somewhere to show them.
        if events:
            if len(sections) == 1:
                cat = next(iter(sections.keys()))
                for e in events:
                    e["category"] = cat
                core_events.extend(events)
            else:
                print(
                    f"warn: {p.name}: {len(events)} @tessera event(s) but "
                    f"{len(sections)} @tessera category declarations — "
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
            sdk_docs[name] = {
                "schema": "tessera-sdk-docs/v1",
                "source": f"cores@{commit}",
                "category": name,
                "label": meta["label"],
                "icon": meta["icon"],
                "header": p.name,
                "functions": docs,
            }

    core_manifest = {
        "schema": "tessera-manifest/v1",
        "source": f"cores@{commit}",
        "categories": core_categories,
        "hosts": core_hosts,
        "events": core_events,
    }

    tile_sources = [
        {
            "path": ROOT / "kiln/drivers/tile_disp_rgbw.h",
            # No matching `Display.RGBW` tile definition in kiln/definitions/
            # today — the placeholder `Display-RGB-a.json` has no
            # interfaces populated. When the real Display.RGBW def lands,
            # point `definition` at it and the address data flows.
            "definition": None,
            "prefix": "tile_disp_rgbw",
            "init": "tile_disp_rgbw_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_i_6p6.h",
            "definition": ROOT / "kiln/definitions/Sense-I-6P6-a.json",
            "prefix": "tile_sense_i_6p6",
            "init": "tile_sense_i_6p6_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_drive_h.h",
            "definition": ROOT / "kiln/definitions/Drive-H-a.json",
            "prefix": "tile_drive_h",
            "init": "tile_drive_h_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_mic.h",
            "definition": ROOT / "kiln/definitions/Sense-MIC-a.json",
            "prefix": "tile_sense_mic",
            "init": "tile_sense_mic_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_i_9.h",
            "definition": ROOT / "kiln/definitions/Sense-I-9-c.json",
            "prefix": "tile_sense_i_9",
            "init": "tile_sense_i_9_init",
            "version": "2.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_t_c.h",
            "definition": ROOT / "kiln/definitions/Sense-T-C-a.json",
            "prefix": "tile_sense_t_c",
            "init": "tile_sense_t_c_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_drive_a_2.h",
            "definition": ROOT / "kiln/definitions/Drive-A-2-a.json",
            "prefix": "tile_drive_a_2",
            "init": "tile_drive_a_2_init",
            "version": "2.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_drive_p.h",
            "definition": ROOT / "kiln/definitions/Drive-P-a.json",
            "prefix": "tile_drive_p",
            "init": "tile_drive_p_init",
            "version": "2.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_power_l_1t.h",
            "definition": ROOT / "kiln/definitions/Power-L-1T-b.json",
            "prefix": "tile_power_l_1t",
            "init": "tile_power_l_1t_init",
            "version": "2.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_bp.h",
            "definition": ROOT / "kiln/definitions/Sense-BP-a.json",
            "prefix": "tile_sense_bp",
            "init": "tile_sense_bp_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_sense_tof.h",
            "definition": ROOT / "kiln/definitions/Sense-TOF-a.json",
            "prefix": "tile_sense_tof",
            "init": "tile_sense_tof_init",
            "version": "1.0.0",
        },
        {
            "path": ROOT / "kiln/drivers/tile_drive_dc_h.h",
            "definition": ROOT / "kiln/definitions/Drive-DC-H-a.json",
            "prefix": "tile_drive_dc_h",
            "init": "tile_drive_dc_h_init",
            "version": "3.0.0",
        },
    ]

    targets = [(CORE_OUT_DIR / "core.json", core_manifest)]

    # Per-category SDK docs JSONs — one file per `@tessera category`.
    for category, doc in sdk_docs.items():
        targets.append((SDK_DOCS_OUT_DIR / f"{category}.json", doc))

    for t in tile_sources:
        hosts, sections, _docs, events = parse_header(t["path"], scope="tile")
        if len(sections) != 1:
            print(
                f"warn: {t['path'].name}: expected exactly one @tessera tile tag, "
                f"found {len(sections)}",
                file=sys.stderr,
            )
        palette = next(iter(sections.values())) if sections else {
            "label": t["path"].stem, "icon": "",
        }
        manifest = {
            "schema": "tessera-manifest/v1",
            "source": f"cores@{commit}",
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
        # Carry per-bus address variants from the kiln tile-def JSON so
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
        drift = []
        for path, data in targets:
            want = serialize(data)
            have = path.read_text() if path.exists() else ""
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
