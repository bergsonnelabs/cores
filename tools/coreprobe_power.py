#!/usr/bin/env python3
"""
coreprobe_power.py — decide and apply target power before an SWD flash.

The CoreProbe can supply the board it is programming. Deciding *whether* and
*at what voltage* is not something software may guess: 5 V into a 1V8 part
destroys it, and a board with an onboard regulator fed 5 V still runs 3V3
logic, so the supply never implies the logic level. Both are declared by the
project, in config.json:

    "probe": { "target_power": "3v3", "target_logic": "3v3" }

`target_power: "off"` (or an absent block) means self-powered: the probe
supplies nothing and senses the board's logic level itself. That is the
default, and what every project that never thought about it means.

Studio reads the same two fields and applies the same rules — see
apps/studio/src/lib/cmsisdap/coreprobe.ts in the web repo. The rules are
deliberately duplicated rather than shared: this file has to run from a bare
SDK checkout with no Node toolchain. Keep `decide()` below in step with
Studio's `connect()`; the tests next door pin the behaviour on this side.

Requires hidapi:  pip install hidapi

    coreprobe_power.py status
    coreprobe_power.py apply --config path/to/config.json
    coreprobe_power.py apply --config … --dry-run
    coreprobe_power.py off
"""
import argparse
import json
import os
import sys
import time

VID, PID = 0x1209, 0xDA01

# DAP_Vendor commands (0x80 + index).
VND_SET_VPAD = 0x80
VND_SENSE_MV = 0x82
VND_SET_VSHIFT = 0x83

VPAD = {"off": 0, "1v8": 1, "3v3": 2, "5v": 3}
VSHIFT = {"1v8": 0x00, "3v3": 0x01}

# Supplies the probe must not drive, and why. Mirrors VPAD_UNAVAILABLE in
# Studio; both are lifted together when the hardware is respun.
UNAVAILABLE = {
    "5v": (
        "5 V back-feeds the probe's own 3V3 rail through the load switch. The "
        "TPS22919's high-side N-channel pass FET has a body diode conducting "
        "OUT->IN (drawn in the datasheet block diagram, mentioned nowhere in "
        "its text), so selecting 5 V pushes ~4.4 V onto VCCB and the target's "
        "SWDIO -- over the level shifter's, the probe MCU's, and the target "
        "pin's ratings. Measured on two probes. Pending the load-switch respin."
    )
}

# Sensed VIO classification, from CoreProbe docs/level-shifter.md. The sense
# reads SWDIO held up by the target's own internal pull-up, so a real target
# sits near 1V8 or near 3V3; anything between is a pull-up that is not properly
# powered and must be reported rather than rounded to the nearer class.
ABSENT_MAX_MV = 300
ONE_V8 = (1400, 2200)
THREE_V3 = (2800, 3600)


def classify(mv):
    if mv < ABSENT_MAX_MV:
        return "absent"
    if ONE_V8[0] <= mv <= ONE_V8[1]:
        return "1v8"
    if THREE_V3[0] <= mv <= THREE_V3[1]:
        return "3v3"
    return "anomalous"


class PowerError(Exception):
    """A refusal the operator has to resolve — never worked around silently."""


def decide(sensed_mv, declared_power, declared_logic):
    """What to do, given what we sensed and what the project declared.

    Pure, so the rules can be tested without a probe attached. Returns
    (supply, logic): `supply` is None when the board powers itself.
    """
    cls = classify(sensed_mv)

    if cls != "absent":
        # Self-powered. Never feed a board that already has a rail — the three
        # load switches share T.V+ and the hardware has no contention
        # protection. The declaration is deliberately ignored here.
        if cls == "anomalous":
            raise PowerError(
                f"Target VIO reads {sensed_mv} mV, which is neither 1V8 "
                f"({ONE_V8[0]}-{ONE_V8[1]}) nor 3V3 ({THREE_V3[0]}-{THREE_V3[1]}). "
                "Check the target's own supply."
            )
        return None, cls

    if not declared_power or declared_power == "off":
        raise PowerError(
            f"Nothing is driving the target's SWDIO ({sensed_mv} mV), so the board "
            "has no power of its own, and this project does not say what to supply.\n"
            "Add to config.json — both fields, neither can be guessed:\n"
            '  "probe": { "target_power": "3v3", "target_logic": "3v3" }\n'
            '  target_power: off | 1v8 | 3v3        target_logic: 1v8 | 3v3'
        )

    if declared_power not in VPAD:
        raise PowerError(f"probe.target_power: '{declared_power}' is not off/1v8/3v3/5v")
    if declared_power in UNAVAILABLE:
        raise PowerError(f"Refusing to supply {declared_power}. {UNAVAILABLE[declared_power]}")
    if declared_logic not in VSHIFT:
        raise PowerError(
            f"probe.target_power is '{declared_power}', so probe.target_logic must be "
            "1v8 or 3v3. The two are independent: a board with its own regulator fed "
            "5 V still runs 3V3 logic, so the supply does not imply the level."
        )
    return declared_power, declared_logic


def read_declaration(config_path):
    """The `probe` block from config.json, or empty when there is none."""
    if not config_path or not os.path.isfile(config_path):
        return None, None
    with open(config_path, encoding="utf-8") as f:
        cfg = json.load(f)
    probe = cfg.get("probe") or {}
    return probe.get("target_power"), probe.get("target_logic")


# ---------- probe transport ----------


def _open():
    import hid  # imported late so --help works without hidapi installed

    d = hid.device()
    d.open(VID, PID)
    d.set_nonblocking(0)
    return d


def _cmd(dev, payload):
    dev.write(bytes([0x00]) + bytes(payload) + bytes(64 - len(payload)))
    return bytes(dev.read(64, 1500))


def sense_mv(dev):
    return int.from_bytes(_cmd(dev, [VND_SENSE_MV])[1:3], "little")


def set_vpad(dev, level):
    _cmd(dev, [VND_SET_VPAD, VPAD[level]])


def set_vshift(dev, level):
    _cmd(dev, [VND_SET_VSHIFT, VSHIFT[level]])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    sub.add_parser("off")
    a = sub.add_parser("apply")
    a.add_argument("--config", required=True, help="path to the project's config.json")
    a.add_argument(
        "--dry-run",
        action="store_true",
        help="sense and report the decision without driving anything",
    )
    args = ap.parse_args()

    try:
        dev = _open()
    except Exception as e:
        print(f"CoreProbe not found ({e}). Is it plugged in?", file=sys.stderr)
        return 2

    try:
        if args.cmd == "off":
            set_vpad(dev, "off")
            print("target supply off")
            return 0

        mv = sense_mv(dev)
        if args.cmd == "status":
            print(f"sensed VIO : {mv} mV ({classify(mv)})")
            return 0

        power, logic = read_declaration(args.config)
        try:
            supply, level = decide(mv, power, logic)
        except PowerError as e:
            print(f"\n{e}\n", file=sys.stderr)
            return 1

        if supply is None:
            print(f"target is self-powered ({mv} mV, {level}) — supplying nothing")
        else:
            print(f"target unpowered ({mv} mV) — supplying {supply}, driving {level} logic")
        if args.dry_run:
            print("(dry run — nothing driven)")
            return 0

        # V_shift first: it is what the shifter drives the target's lines at, and
        # it must be right before anything is powered up beside it.
        set_vshift(dev, level)
        if supply is not None:
            set_vpad(dev, supply)
            time.sleep(0.3)
            after = sense_mv(dev)
            # Only checks that something came up. The value cannot pick the
            # level once we are the supply — see the note in Studio's connect().
            if classify(after) == "absent":
                set_vpad(dev, "off")
                print(
                    f"Supplied {supply} but SWDIO still reads {after} mV — the target "
                    "never came up. Check the strap between probe and target.",
                    file=sys.stderr,
                )
                return 1
            print(f"rail came up ({after} mV sensed)")
        return 0
    finally:
        dev.close()


if __name__ == "__main__":
    sys.exit(main())
