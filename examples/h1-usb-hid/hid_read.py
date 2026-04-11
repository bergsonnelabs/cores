#!/usr/bin/env python3
"""
hid_read.py — Read HID reports from a Core tile composite device.

Reads 64-byte vendor-defined HID reports (VID:1209 PID:0001)
and decodes the first 6 bytes as a counter + sensor value.

Report layout (matching the firmware's hid_report_t):
    [0:3]  uint32_t  counter (little-endian)
    [4:5]  uint16_t  sensor value
    [6:63] zero padding

Requires: hidapi (pip3 install hidapi)
"""

import sys
import struct
import time

try:
    import hid
except ImportError:
    print("Error: hidapi not installed. Run: pip3 install hidapi")
    sys.exit(1)

VID = 0x1209
PID = 0x0001


def main():
    print(f"Looking for device VID={VID:#06x} PID={PID:#06x}...")

    device = hid.device()
    try:
        device.open(VID, PID)
    except OSError as e:
        print(f"Cannot open device: {e}")
        print("Make sure the Core tile is connected and firmware is running.")
        sys.exit(1)

    device.set_nonblocking(False)
    print(f"Connected: {device.get_manufacturer_string()} {device.get_product_string()}")
    print(f"\n{'Counter':>10s}  {'Sensor':>8s}")
    print("-" * 22)

    try:
        while True:
            data = device.read(64, timeout_ms=1000)
            if not data:
                continue

            if len(data) >= 6:
                counter, sensor = struct.unpack_from("<IH", bytes(data))
                print(f"{counter:>10d}  {sensor:>8d}")

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        device.close()


if __name__ == "__main__":
    main()
