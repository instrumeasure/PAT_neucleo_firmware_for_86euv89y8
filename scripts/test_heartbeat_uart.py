"""Read ST-Link VCP and print HB lines (matches main.c printf format). Default COM from first STLink in pio list, else COM10."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time

HB_RE = re.compile(r"^HB,.+,.+,.+,.+,.+,.+,.+$")


def self_test() -> int:
    samples = (
        "HB,RUN,12345,0,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF",
        "HB,INIT,99,42,0xABCDEF,0x123456,0xFFFFFF,0xFFFFFF",
    )
    for s in samples:
        if not HB_RE.match(s):
            print(f"FAIL regex: {s!r}", file=sys.stderr)
            return 1
    print("# self-test: HB line regex OK")
    return 0


def guess_stlink_com() -> str | None:
    try:
        out = subprocess.run(
            [sys.executable, "-m", "platformio", "device", "list"],
            capture_output=True,
            text=True,
            timeout=45,
            cwd=None,
        )
        for line in out.stdout.splitlines():
            line = line.strip()
            if "STMicroelectronics STLink Virtual COM Port" in line and "(" in line:
                start = line.rfind("(") + 1
                end = line.rfind(")")
                if start > 0 and end > start:
                    return line[start:end]
    except (OSError, subprocess.TimeoutExpired, ValueError):
        pass
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description="Sniff PAT firmware HB UART lines.")
    ap.add_argument("--port", "-p", default="", help="COM port e.g. COM10")
    ap.add_argument("--baud", "-b", type=int, default=115200)
    ap.add_argument("--seconds", "-s", type=float, default=14.0)
    ap.add_argument("--max-lines", "-n", type=int, default=10)
    ap.add_argument("--self-test", action="store_true", help="Validate parser only (no serial).")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    port = args.port or guess_stlink_com() or "COM10"

    try:
        import serial
    except ImportError:
        print("Install pyserial: py -m pip install pyserial", file=sys.stderr)
        return 2

    try:
        ser = serial.Serial(port, args.baud, timeout=1.0)
    except serial.serialutil.SerialException as e:  # type: ignore[attr-defined]
        print(
            f"Cannot open {port}: {e}\n"
            "Close other serial monitors, STM32CubeProgrammer UART, or any app using the ST-Link VCP.",
            file=sys.stderr,
        )
        return 3
    print(f"# Opened {port} @ {args.baud}", file=sys.stderr)
    t0 = time.time()
    buf = bytearray()
    lines_ok = 0
    lines_bad = 0

    try:
        while time.time() - t0 < args.seconds and lines_ok < args.max_lines:
            chunk = ser.read(4096)
            if chunk:
                buf.extend(chunk)
            while True:
                i = buf.find(b"\n")
                if i < 0:
                    break
                line = bytes(buf[:i])
                del buf[: i + 1]
                if line.endswith(b"\r"):
                    line = line[:-1]
                try:
                    text = line.decode("utf-8")
                except UnicodeDecodeError:
                    text = line.decode("latin-1")
                if text.startswith("HB,"):
                    if HB_RE.match(text):
                        print(text)
                        lines_ok += 1
                    else:
                        print(f"# malformed: {text!r}", file=sys.stderr)
                        lines_bad += 1
                elif text.strip():
                    print(f"# skip: {text!r}", file=sys.stderr)
    finally:
        ser.close()

    print(f"# summary: ok={lines_ok} malformed={lines_bad}", file=sys.stderr)
    return 0 if lines_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
