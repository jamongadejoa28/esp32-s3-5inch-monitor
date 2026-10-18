"""Exercise the PC utility through a pseudo-terminal, without touching a board."""
import importlib.util
import os
from pathlib import Path
import pty
import select
import threading
import time

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("monitor_sync_time", ROOT / "tools/sync_time.py")
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


def test_pc_time_is_captured_after_ready():
    master, slave = pty.openpty()
    port = os.ttyname(slave)
    received = []
    failures = []
    ready_time = []

    def firmware():
        try:
            buffer = b""
            end = time.monotonic() + 3
            while time.monotonic() < end:
                if not select.select([master], [], [], 0.1)[0]:
                    continue
                buffer += os.read(master, 512)
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    line = line.strip()
                    if line == b"MEX_TIME_GET":
                        # A stray old acknowledgment must not be treated as a successful sync.
                        os.write(master, b"MEX_TIME_OK\n")
                        time.sleep(0.1)
                        ready_time.append(time.time_ns() // 1_000_000)
                        os.write(master, b"MEX_TIME_READY 123456\n")
                    elif line.startswith(b"MEX_TIME_SET "):
                        _, token, timestamp = line.split()
                        assert token == b"123456"
                        received.append(int(timestamp))
                        os.write(master, b"MEX_TIME_OK\n")
                        return
            raise AssertionError("PC did not send time")
        except BaseException as error:
            failures.append(error)

    worker = threading.Thread(target=firmware)
    worker.start()
    try:
        sync.sync_time(port, timeout=3)
        worker.join(3)
        assert not worker.is_alive() and not failures
        assert len(received) == 1
        assert ready_time[0] <= received[0] <= time.time_ns() // 1_000_000
    finally:
        worker.join(3)
        os.close(master)
        os.close(slave)


def test_missing_firmware_times_out():
    master, slave = pty.openpty()
    try:
        with pytest.raises(TimeoutError, match="No time-sync acknowledgment"):
            sync.sync_time(os.ttyname(slave), timeout=0.2)
    finally:
        os.close(master)
        os.close(slave)
