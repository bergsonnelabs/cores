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
    r"^[ \t]*(?:static\s+inline\s+)?([\w\s\*]+?)\s+(\w+)\s*\(([^;{]*)\)",
    re.MULTILINE,
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


def parse_tessera_tag(lines):
    for line in lines:
        m = re.match(r"@tessera\s+expose\s*(.*)", line.strip())
        if not m:
            continue
        attrs = {}
        for tok in m.group(1).split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                attrs[k] = v
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
    tail = source[after_offset:]
    m = SIG_RE.search(tail)
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


def build_host_entry(tag, doxy_lines, sig, header_name, scope):
    description = first_brief(doxy_lines)
    doxy_params = parse_params(doxy_lines)

    receiver = None
    c_params = []
    for sp in sig["params"]:
        norm = sp["ctype"].replace(" *", "*").replace("* ", "*").strip()
        if norm == "tile_t*":
            receiver = sp["ctype"]
        else:
            c_params.append(sp)

    if len(doxy_params) != len(c_params):
        print(
            f"warn: {sig['name']}: @param count {len(doxy_params)} != C arg count {len(c_params)}",
            file=sys.stderr,
        )

    dsl_params = []
    for i, cp in enumerate(c_params):
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
    if "icon" in tag:
        host["icon"] = tag["icon"]
    if receiver:
        host["receiver"] = receiver.strip()
    if "availability" in tag:
        host["availability"] = {"cores": tag["availability"].split(",")}
    return host


def parse_header(path, scope):
    source = path.read_text()
    entries = []
    for m in DOXY_BLOCK_RE.finditer(source):
        lines = strip_doxy(m.group(1))
        tag = parse_tessera_tag(lines)
        if not tag:
            continue
        sig = extract_signature(source, m.end())
        if not sig:
            print(f"warn: {path.name}: no signature after doxy block", file=sys.stderr)
            continue
        entries.append(build_host_entry(tag, lines, sig, path.name, scope))
    return entries


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if manifests on disk are out of sync")
    args = ap.parse_args()

    commit = source_commit()

    core_sources = [
        ROOT / "sdk/hal/hal_led.h",
        ROOT / "sdk/core/core_usb.h",
    ]
    core_hosts = []
    for p in core_sources:
        core_hosts.extend(parse_header(p, scope="core"))

    core_manifest = {
        "schema": "tessera-manifest/v1",
        "source": f"cores@{commit}",
        "hosts": core_hosts,
        "events": [],
    }

    tile_sources = [
        {
            "path": ROOT / "kiln/drivers/tile_disp_rgbw.h",
            "tile": "Disp.RGBW",
            "prefix": "tile_disp_rgbw",
            "init": "tile_disp_rgbw_init",
            "version": "1.0.0",
        },
    ]

    targets = [(CORE_OUT_DIR / "core.json", core_manifest)]
    for t in tile_sources:
        manifest = {
            "schema": "tessera-manifest/v1",
            "source": f"cores@{commit}",
            "tile": t["tile"],
            "driver": {
                "prefix": t["prefix"],
                "header": t["path"].name,
                "version": t["version"],
            },
            "handle": {"type": "tile_t", "init": t["init"]},
            "hosts": parse_header(t["path"], scope="tile"),
            "events": [],
        }
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
        print(
            f"wrote {path.relative_to(ROOT)}  "
            f"({len(data['hosts'])} hosts, {len(data.get('events', []))} events)"
        )


if __name__ == "__main__":
    main()
