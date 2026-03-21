#!/usr/bin/env python3
"""
tilegen — Generate C headers from Mosaic tile JSON definitions.

Usage:
    python3 tilegen.py <tile.json> [output_dir]

Reads a tile JSON file and produces:
    tile_pins.h        Pad-to-GPIO mapping defines
    tile_board.h       Board-level defines (LED, power, debug)
    tile_interfaces.h  Interface convenience defines with AF numbers

Output defaults to ./generated/ if not specified.
"""

import json
import os
import re
import sys
from pathlib import Path

from jinja2 import Environment, FileSystemLoader

# ---- MCU database ----
# Maps part numbers to build-relevant properties.

MCU_DB = {
    "STM32L011E4": {
        "define": "STM32L011xx",
        "family": "stm32l0xx",
        "core": "cortex-m0plus",
        "cpu_flag": "-mcpu=cortex-m0plus",
        "fpu": None,
    },
    "STM32L422TB": {
        "define": "STM32L422xx",
        "family": "stm32l4xx",
        "core": "cortex-m4",
        "cpu_flag": "-mcpu=cortex-m4",
        "fpu": "fpv4-sp-d16",
    },
    "STM32WBA55HGF6": {
        "define": "STM32WBA55xx",
        "family": "stm32wbaxx",
        "core": "cortex-m33",
        "cpu_flag": "-mcpu=cortex-m33",
        "fpu": "fpv5-sp-d16",
    },
    "STM32H523HE": {
        "define": "STM32H523xx",
        "family": "stm32h5xx",
        "core": "cortex-m33",
        "cpu_flag": "-mcpu=cortex-m33",
        "fpu": "fpv5-sp-d16",
    },
}


def parse_gpio(function_str):
    """Parse a digital function name like 'A7' or 'B12' into (port, pin) or (None, None)."""
    m = re.match(r'^([A-H])(\d+)$', function_str)
    if m:
        return m.group(1), int(m.group(2))
    # Handle PH3 style (some JSONs use 'H3' or 'PH3')
    m = re.match(r'^P?([A-H])(\d+)$', function_str)
    if m:
        return m.group(1), int(m.group(2))
    return None, None


def extract_pad_gpio(pad):
    """Extract the default GPIO port/pin from a pad's functions list."""
    for func in pad.get("functions", []):
        if func.get("type") == "digital":
            port, pin = parse_gpio(func["function"])
            if port is not None:
                return port, pin
    return None, None


def extract_system_pads(pads):
    """Find system pads: SWCLK, SWDIO, BOOT0, NRST, V+, GND."""
    system = {}
    for pad in pads:
        for func in pad.get("functions", []):
            fname = func.get("function", "")
            if fname in ("SWCLK", "SWDIO", "BOOT0", "NRST", "GND", "V+"):
                key = fname.replace("+", "PLUS")
                system[key] = pad["pad"]
    return system


def extract_led_info(tile):
    """Extract LED pin info from application_notes."""
    for note in tile.get("application_notes", []):
        text = note.get("details", "") + " " + note.get("heading", "")
        if "LED" in text.upper():
            # Look for patterns like "PA8", "PB12", etc.
            m = re.search(r'P([A-H])(\d+)', note.get("details", ""))
            if m:
                port = m.group(1)
                pin = int(m.group(2))
                active_high = "active-high" in note.get("details", "").lower()
                return {"port": port, "pin": pin, "active_high": active_high}
    return None


def build_pad_map(pads):
    """Build a list of pad info dicts for template rendering."""
    pad_map = []
    for pad in pads:
        port, pin = extract_pad_gpio(pad)

        # Collect AF functions
        af_functions = []
        for func in pad.get("functions", []):
            if "af" in func:
                af_functions.append({
                    "function": func["function"],
                    "type": func["type"],
                    "af": func["af"],
                })

        # Classify the pad
        pad_type = "gpio"
        for func in pad.get("functions", []):
            if func.get("function") in ("GND", "V+"):
                pad_type = "power"
                break
            if func.get("function") == "NRST":
                pad_type = "system"
                break

        pad_map.append({
            "number": pad["pad"],
            "port": port,
            "pin": pin,
            "type": pad_type,
            "af_functions": af_functions,
        })

    return pad_map


def sanitize_signal_name(name):
    """Make a signal name safe for C identifiers."""
    name = name.replace("+", "P").replace("-", "M")
    name = re.sub(r'[^A-Za-z0-9_]', '_', name)
    return name


def build_interface_map(tile, pad_map):
    """Build interface info with resolved GPIO ports/pins/AFs.

    When multiple pads map to the same signal (e.g., SPI1.CLK on pads 3 and 10),
    only the first is_required pad is used for the primary define. Alternates are
    emitted with a _ALT<n> suffix.
    """
    # Create a lookup: pad_number -> pad_info
    pad_lookup = {p["number"]: p for p in pad_map}

    interfaces = []
    for iface in tile.get("interfaces", []):
        signals = []
        seen_signals = {}  # signal_name -> count

        for assign in iface.get("pad_assignments", []):
            pad_num = assign["pad"]
            pad_info = pad_lookup.get(pad_num, {})
            fname = assign["function"]

            # Find the AF for this specific function on this pad
            af = None
            for af_func in pad_info.get("af_functions", []):
                if af_func["function"] == fname:
                    af = af_func["af"]
                    break

            raw_signal = fname.split(".")[-1] if "." in fname else fname
            signal = sanitize_signal_name(raw_signal)

            # Handle duplicate signals — first required gets the base name,
            # subsequent get _ALT, _ALT2, etc.
            is_required = assign.get("is_required", False)
            if signal in seen_signals:
                seen_signals[signal] += 1
                signal = f"{signal}_ALT{seen_signals[signal]}" if seen_signals[signal] > 1 else f"{signal}_ALT"
            else:
                seen_signals[signal] = 0

            signals.append({
                "pad": pad_num,
                "function": fname,
                "signal": signal,
                "port": pad_info.get("port"),
                "pin": pad_info.get("pin"),
                "af": af,
                "is_required": is_required,
            })

        interfaces.append({
            "name": iface["name"],
            "type": iface["type"],
            "parameters": iface.get("parameters", {}),
            "signals": signals,
        })

    return interfaces


def generate(tile_path, output_dir):
    """Generate all headers from a tile JSON."""
    with open(tile_path) as f:
        tile = json.load(f)

    # Resolve MCU info
    part = tile["components"][0]["part"]
    mcu = MCU_DB.get(part)
    if mcu is None:
        print(f"ERROR: Unknown MCU part '{part}'. Add it to MCU_DB in tilegen.py.")
        sys.exit(1)

    # Build template context
    pad_map = build_pad_map(tile["pads"])
    system_pads = extract_system_pads(tile["pads"])
    led = extract_led_info(tile)
    interfaces = build_interface_map(tile, pad_map)
    power = tile.get("power", [{}])[0] if tile.get("power") else {}

    tile_name = f"{tile['family']}.{tile['name']}"

    ctx = {
        "tile": tile,
        "tile_name": tile_name,
        "tile_family": tile["family"],
        "tile_variant": tile["name"],
        "tile_rev": tile["rev"],
        "tile_headline": tile.get("headline", ""),
        "json_version": tile.get("json_version", ""),
        "mcu_part": part,
        "mcu": mcu,
        "pad_count": tile["package"]["pads"],
        "pads": pad_map,
        "system_pads": system_pads,
        "led": led,
        "interfaces": interfaces,
        "power": power,
        "source_file": os.path.basename(tile_path),
    }

    # Set up Jinja2
    templates_dir = os.path.join(os.path.dirname(__file__), "templates")
    env = Environment(
        loader=FileSystemLoader(templates_dir),
        keep_trailing_newline=True,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    # Generate each template
    os.makedirs(output_dir, exist_ok=True)

    for template_name in ["tile_pins.h.j2", "tile_board.h.j2", "tile_interfaces.h.j2"]:
        out_name = template_name.replace(".j2", "")
        template = env.get_template(template_name)
        output = template.render(**ctx)
        out_path = os.path.join(output_dir, out_name)
        with open(out_path, "w") as f:
            f.write(output)
        print(f"  {out_name}")

    print(f"  -> {output_dir}/")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <tile.json> [output_dir]")
        sys.exit(1)

    tile_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "generated"

    if not os.path.exists(tile_path):
        print(f"ERROR: File not found: {tile_path}")
        sys.exit(1)

    tile_name = os.path.basename(tile_path).replace(".json", "")
    print(f"tilegen: {tile_name}")
    generate(tile_path, output_dir)


if __name__ == "__main__":
    main()
