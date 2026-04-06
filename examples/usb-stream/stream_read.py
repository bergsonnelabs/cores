#!/usr/bin/env python3
"""
stream_read.py — Start/stop raw binary streaming from a Core tile.

Sends 's' to start streaming and 'x' to stop. Displays live
throughput and decoded sample values. Ctrl+C to quit.

Usage:
    python3 stream_read.py [port]

    port defaults to the first /dev/tty.usbmodem* (macOS) or
    /dev/ttyACM0 (Linux) found.

Packet format (6 bytes):
    [0:3]  uint32_t  sequence number (little-endian)
    [4:5]  uint16_t  sample value

Requires: pyserial (pip3 install pyserial)
"""

import sys
import time
import glob
import struct

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip3 install pyserial")
    sys.exit(1)

PACKET_SIZE = 6


def find_port():
    """Auto-detect the Core tile serial port."""
    for pattern in ["/dev/tty.usbmodem*", "/dev/ttyACM*"]:
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        print("No USB serial port found. Pass the port as an argument.")
        sys.exit(1)

    print(f"Opening {port}...")
    ser = serial.Serial(port, baudrate=115200, timeout=0.1)

    # Flush any stale data
    ser.reset_input_buffer()

    # Start streaming
    ser.write(b"s")
    print("Sent 's' — streaming started.\n")

    total_bytes = 0
    total_packets = 0
    last_time = time.time()
    buf = b""

    print(f"{'Rate':>10s}  {'Packets':>10s}  {'Last Seq':>10s}  {'Last Sample':>12s}")
    print("-" * 50)

    try:
        while True:
            data = ser.read(4096)
            if not data:
                continue

            total_bytes += len(data)
            buf += data

            # Parse complete packets from the buffer
            seq = 0
            sample = 0
            while len(buf) >= PACKET_SIZE:
                seq, sample = struct.unpack_from("<IH", buf)
                total_packets += 1
                buf = buf[PACKET_SIZE:]

            # Print stats every second
            now = time.time()
            elapsed = now - last_time
            if elapsed >= 1.0:
                rate = total_bytes / elapsed
                if rate > 1_000_000:
                    rate_str = f"{rate / 1_000_000:.2f} MB/s"
                else:
                    rate_str = f"{rate / 1_000:.1f} KB/s"

                print(f"{rate_str:>10s}  {total_packets:>10d}  {seq:>10d}  {sample:>12d}")

                total_bytes = 0
                last_time = now

    except KeyboardInterrupt:
        # Stop streaming before closing
        ser.write(b"x")
        time.sleep(0.1)
        print("\nSent 'x' — streaming stopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
