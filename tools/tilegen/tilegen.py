#!/usr/bin/env python3
"""
tilegen — Generate C headers from Mosaic tile JSON definitions.

Usage:
    python3 tilegen.py <tile.json> [output_dir] [--project project.json]

Reads a tile JSON file and produces:
    tile_pins.h        Pad-to-GPIO mapping defines
    tile_board.h       Board-level defines (LED, power, debug)
    tile_interfaces.h  Interface convenience defines with AF numbers
    tile_config.h      Project-specific pin and clock configuration
                       (only when --project is provided)

Output defaults to ./generated/ if not specified.
"""

import argparse
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
        "clock_sources": ["hsi16", "msi", "hse"],
        "default_clock": "hsi16",
        "default_mhz": 16,
    },
    "STM32L422TB": {
        "define": "STM32L422xx",
        "family": "stm32l4xx",
        "core": "cortex-m4",
        "cpu_flag": "-mcpu=cortex-m4",
        "fpu": "fpv4-sp-d16",
        "clock_sources": ["hsi16", "msi", "hse"],
        "default_clock": "hsi16",
        "default_mhz": 16,
    },
    "STM32WBA55HGF6": {
        "define": "STM32WBA55xx",
        "family": "stm32wbaxx",
        "core": "cortex-m33",
        "cpu_flag": "-mcpu=cortex-m33",
        "fpu": "fpv5-sp-d16",
        "clock_sources": ["hsi16", "hse"],
        "default_clock": "hsi16",
        "default_mhz": 16,
    },
    "STM32H523HE": {
        "define": "STM32H523xx",
        "family": "stm32h5xx",
        "core": "cortex-m33",
        "cpu_flag": "-mcpu=cortex-m33",
        "fpu": "fpv5-sp-d16",
        "clock_sources": ["hsi48", "hse", "csi"],
        "default_clock": "hsi48",
        "default_mhz": 48,
    },
}


def parse_gpio(function_str):
    """Parse a digital function name like 'A7' or 'B12' into (port, pin) or (None, None)."""
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

        # Collect all available functions
        all_functions = []
        af_functions = []
        for func in pad.get("functions", []):
            all_functions.append(func["function"])
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
            "all_functions": all_functions,
            "af_functions": af_functions,
        })

    return pad_map


def sanitize_signal_name(name):
    """Make a signal name safe for C identifiers."""
    name = name.replace("+", "P").replace("-", "M")
    name = re.sub(r'[^A-Za-z0-9_]', '_', name)
    return name


def build_interface_map(tile, pad_map):
    """Build interface info with resolved GPIO ports/pins/AFs."""
    pad_lookup = {p["number"]: p for p in pad_map}

    interfaces = []
    for iface in tile.get("interfaces", []):
        signals = []
        seen_signals = {}

        for assign in iface.get("pad_assignments", []):
            pad_num = assign["pad"]
            pad_info = pad_lookup.get(pad_num, {})
            fname = assign["function"]

            af = None
            for af_func in pad_info.get("af_functions", []):
                if af_func["function"] == fname:
                    af = af_func["af"]
                    break

            raw_signal = fname.split(".")[-1] if "." in fname else fname
            signal = sanitize_signal_name(raw_signal)

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


# ---- Project config validation ----

def validate_project_config(config, tile, pad_map):
    """Validate a project config against a tile definition.

    Returns (warnings, errors) where each is a list of strings.
    """
    warnings = []
    errors = []

    # Build lookup of available functions per pad
    pad_lookup = {p["number"]: p for p in pad_map}

    # Validate pin assignments
    pins = config.get("pins", {})
    for pad_num, assigned_func in pins.items():
        if pad_num not in pad_lookup:
            errors.append(f"Pad {pad_num}: does not exist on this tile (has {len(pad_lookup)} pads)")
            continue

        pad_info = pad_lookup[pad_num]

        # GPIO.OUT and GPIO.IN are synthetic — always valid on GPIO pads
        if assigned_func in ("GPIO.OUT", "GPIO.IN"):
            if pad_info["port"] is None:
                errors.append(f"Pad {pad_num}: cannot use {assigned_func} on a non-GPIO pad")
            continue

        # Check if the assigned function exists on this pad
        if assigned_func not in pad_info["all_functions"]:
            available = [f for f in pad_info["all_functions"]
                         if f not in ("GND", "V+", "NRST")]
            errors.append(
                f"Pad {pad_num}: '{assigned_func}' is not available. "
                f"Options: {', '.join(available)}"
            )

    # Validate interface configs reference real interfaces
    iface_names = {i["name"] for i in tile.get("interfaces", [])}
    for iface_name in config.get("interfaces", {}):
        if iface_name not in iface_names:
            errors.append(
                f"Interface '{iface_name}': not found on this tile. "
                f"Available: {', '.join(sorted(iface_names))}"
            )

    # Check that pin assignments are consistent with interface configs
    configured_ifaces = set(config.get("interfaces", {}).keys())
    assigned_ifaces = set()
    for pad_num, func in pins.items():
        if "." in func and func not in ("GPIO.OUT", "GPIO.IN"):
            iface = func.split(".")[0]
            assigned_ifaces.add(iface)

    for iface in configured_ifaces - assigned_ifaces:
        warnings.append(
            f"Interface '{iface}' configured but no pins assigned to it"
        )

    # Validate clock source against tile (not chip — tile may have crystals)
    clock = config.get("clock", {})
    if clock:
        tile_clocks = tile.get("clock", {})
        available = [s["type"] for s in tile_clocks.get("sources", [])]
        source = clock.get("source", "")
        if source and available and source not in available:
            errors.append(
                f"Clock source '{source}' not available on this tile. "
                f"Options: {', '.join(available)}"
            )

    return warnings, errors


def build_pin_config(config, pad_map):
    """Build the resolved pin configuration from project config.

    For each assigned pin, resolves the GPIO port/pin and AF number.
    """
    pad_lookup = {p["number"]: p for p in pad_map}
    pin_configs = []

    for pad_num, assigned_func in config.get("pins", {}).items():
        pad_info = pad_lookup.get(pad_num)
        if pad_info is None:
            continue

        entry = {
            "pad": pad_num,
            "function": assigned_func,
            "port": pad_info["port"],
            "pin": pad_info["pin"],
            "af": None,
            "mode": "af",  # alternate function
        }

        if assigned_func == "GPIO.OUT":
            entry["mode"] = "output"
            entry["af"] = None
        elif assigned_func == "GPIO.IN":
            entry["mode"] = "input"
            entry["af"] = None
        else:
            # Find AF for this function
            for af_func in pad_info["af_functions"]:
                if af_func["function"] == assigned_func:
                    entry["af"] = af_func["af"]
                    break

        pin_configs.append(entry)

    return pin_configs


def build_clock_config(config, tile):
    """Build resolved clock configuration with defaults from tile JSON."""
    clock = config.get("clock", {})
    tile_clock = tile.get("clock", {})

    # Default source from tile definition
    default_source = tile_clock.get("default", "hsi16")

    # Find frequency for the selected source
    source = clock.get("source", default_source)
    default_mhz = 16
    for src in tile_clock.get("sources", []):
        if src["type"] == source:
            default_mhz = src["frequency_mhz"]
            break

    return {
        "source": source,
        "sysclk_mhz": clock.get("sysclk_mhz", default_mhz),
        "pll": clock.get("pll"),
        "ahb_div": clock.get("ahb_div", 1),
        "apb1_div": clock.get("apb1_div", 1),
        "apb2_div": clock.get("apb2_div", 1),
    }


# ---- Generation ----

def generate(tile_path, output_dir, project_path=None):
    """Generate all headers from a tile JSON and optional project config."""
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

    # Load and validate project config if provided
    templates = ["tile_pins.h.j2", "tile_board.h.j2", "tile_interfaces.h.j2"]

    if project_path:
        with open(project_path) as f:
            project = json.load(f)

        # Validate tile matches
        proj_tile = project.get("project", {}).get("tile", "")
        tile_file_stem = os.path.basename(tile_path).replace(".json", "")
        if proj_tile and proj_tile != tile_file_stem:
            print(f"ERROR: project.json targets '{proj_tile}' but tile is '{tile_file_stem}'")
            sys.exit(1)

        # Validate pin/interface/clock assignments
        warnings, errors = validate_project_config(project, tile, pad_map)
        for w in warnings:
            print(f"  WARNING: {w}")
        if errors:
            for e in errors:
                print(f"  ERROR: {e}")
            sys.exit(1)

        # Build resolved configs
        ctx["project"] = project.get("project", {})
        ctx["pin_config"] = build_pin_config(project, pad_map)
        ctx["clock_config"] = build_clock_config(project, tile)
        ctx["iface_config"] = project.get("interfaces", {})
        ctx["project_file"] = os.path.basename(project_path)

        templates.append("tile_config.h.j2")

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

    for template_name in templates:
        out_name = template_name.replace(".j2", "")
        template = env.get_template(template_name)
        output = template.render(**ctx)
        out_path = os.path.join(output_dir, out_name)
        with open(out_path, "w") as f:
            f.write(output)
        print(f"  {out_name}")

    print(f"  -> {output_dir}/")


def main():
    parser = argparse.ArgumentParser(
        description="Generate C headers from Mosaic tile JSON definitions."
    )
    parser.add_argument("tile_json", help="Path to the tile JSON definition")
    parser.add_argument("output_dir", nargs="?", default="generated",
                        help="Output directory (default: generated)")
    parser.add_argument("--project", "-p", metavar="FILE",
                        help="Path to project.json config file")

    args = parser.parse_args()

    if not os.path.exists(args.tile_json):
        print(f"ERROR: File not found: {args.tile_json}")
        sys.exit(1)

    if args.project and not os.path.exists(args.project):
        print(f"ERROR: Project config not found: {args.project}")
        sys.exit(1)

    tile_name = os.path.basename(args.tile_json).replace(".json", "")
    print(f"tilegen: {tile_name}")
    generate(args.tile_json, args.output_dir, args.project)


if __name__ == "__main__":
    main()
