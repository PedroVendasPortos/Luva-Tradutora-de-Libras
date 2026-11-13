#!/usr/bin/env python3
"""Capture the firmware's CSV stream over USB serial.

Expected firmware commands: START and PAUSE.
The firmware must emit a CSV header beginning with session_id,sample_id,timestamp_ms.
Diagnostic lines beginning with # are ignored.
"""
from __future__ import annotations

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial import SerialException
except ImportError:
    print("pyserial is missing. Install it with: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Capture ESP32 sensor samples into a labelled CSV file.")
    parser.add_argument("port", help="Serial port, e.g. COM3")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--label", required=True, help="Gesture label for this capture, e.g. P")
    parser.add_argument("--output-dir", type=Path, default=Path("data/raw"), help="Output directory")
    parser.add_argument("--duration", type=float, default=0, help="Stop after N seconds; 0 means until Ctrl+C")
    parser.add_argument("--startup-wait", type=float, default=2.5, help="Wait after opening serial (ESP32 reset)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    label = args.label.strip().upper()
    if not label or any(ch in label for ch in ",\r\n"):
        print("Invalid label.", file=sys.stderr)
        return 2
    if args.baud <= 0 or args.duration < 0:
        print("Baud must be positive and duration cannot be negative.", file=sys.stderr)
        return 2

    args.output_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_path = args.output_dir / f"gesture_{label}_{stamp}.csv"

    print(f"Opening {args.port} at {args.baud} baud.")
    print("Close Arduino Serial Monitor/Plotter before continuing.")
    try:
        with serial.Serial(args.port, args.baud, timeout=1, write_timeout=2) as device:
            time.sleep(max(0, args.startup_wait))
            device.reset_input_buffer()
            device.write(b"START\n")
            device.flush()
            print("START sent. Keep the hand pose stable; press Ctrl+C to stop.")

            header: list[str] | None = None
            rows_saved = 0
            malformed = 0
            started_at = time.monotonic()

            with output_path.open("w", newline="", encoding="utf-8") as output:
                writer = None
                try:
                    while True:
                        if args.duration and time.monotonic() - started_at >= args.duration:
                            break
                        raw = device.readline()
                        if not raw:
                            continue
                        line = raw.decode("utf-8", errors="replace").strip()
                        if not line or line.startswith("#"):
                            continue

                        try:
                            fields = next(csv.reader([line]))
                        except (csv.Error, StopIteration):
                            malformed += 1
                            continue

                        if header is None:
                            if len(fields) >= 3 and fields[:3] == ["session_id", "sample_id", "timestamp_ms"]:
                                header = fields
                                if len(set(header)) != len(header):
                                    raise RuntimeError("CSV header contains duplicate column names.")
                                writer = csv.writer(output)
                                writer.writerow(header + ["gesture_label"])
                                output.flush()
                                print(f"CSV header received ({len(header)} columns).")
                            continue

                        if len(fields) != len(header):
                            malformed += 1
                            print(f"Skipping malformed row: expected {len(header)} columns, received {len(fields)}.")
                            continue

                        # Check identifiers and timestamp without altering sensor values.
                        try:
                            int(fields[0])
                            int(fields[1])
                            int(fields[2])
                        except ValueError:
                            malformed += 1
                            continue

                        assert writer is not None
                        writer.writerow(fields + [label])
                        rows_saved += 1
                        if rows_saved % 20 == 0:
                            output.flush()
                            print(f"\rSaved {rows_saved} samples...", end="", flush=True)
                except KeyboardInterrupt:
                    print("\nStopping capture...")
                finally:
                    try:
                        device.write(b"PAUSE\n")
                        device.flush()
                    except SerialException:
                        pass

            if header is None:
                print("No CSV header received. Check the firmware, port, and that START is accepted.", file=sys.stderr)
                output_path.unlink(missing_ok=True)
                return 1

    except SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        print("Check the COM port and close Arduino Serial Monitor/Plotter.", file=sys.stderr)
        return 1

    print(f"Capture complete: {rows_saved} samples saved.")
    print(f"Malformed rows skipped: {malformed}")
    print(f"File: {output_path.resolve()}")
    if rows_saved == 0:
        print("Warning: no samples were captured. Do not use this file for training.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
