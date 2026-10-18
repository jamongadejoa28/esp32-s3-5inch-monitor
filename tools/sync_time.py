"""Synchronize an already-running monitor from the PC clock over USB Serial/JTAG."""
import argparse
from datetime import datetime, timezone, timedelta
import re
import time

import serial


def sync_time(port: str, timeout: float = 90) -> None:
    deadline = time.monotonic() + timeout
    with serial.Serial(port, 115200, timeout=0.2, write_timeout=2, exclusive=True) as device:
        next_query = 0.0
        sent = False
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_query and not sent:
                device.write(b"\nMEX_TIME_GET\n")
                next_query = now + 2
            line = device.readline().decode("utf-8", errors="replace").strip()
            match = re.fullmatch(r"MEX_TIME_READY (\d+)", line)
            if match:
                token = match.group(1)
                # Capture wall time after the device responds, not before boot/calibration.
                epoch_ms = time.time_ns() // 1_000_000
                device.write(f"MEX_TIME_SET {token} {epoch_ms}\n".encode())
                device.flush()
                sent = True
            elif line == "MEX_TIME_OK" and sent:
                korean = datetime.now(timezone(timedelta(hours=9)))
                print(f"PC time synchronized: {korean:%Y-%m-%d %H:%M:%S} KST ({port})")
                return
            elif line.startswith("MEX_TIME_ERROR"):
                sent = False
                next_query = 0
        raise TimeoutError(
            "No time-sync acknowledgment. Close the ESP-IDF Monitor, flash the updated firmware, "
            "and finish touch calibration before retrying."
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-p", "--port", required=True)
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    try:
        sync_time(args.port, args.timeout)
    except (serial.SerialException, OSError, TimeoutError) as error:
        parser.exit(1, f"Time sync failed: {error}\n")


if __name__ == "__main__":
    main()
