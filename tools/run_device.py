"""Flash, synchronize PC time, then open the IDF monitor (locked project Python)."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from sync_time import sync_time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("-p", "--port")
args = parser.parse_args()
port = args.port or json.loads((root / ".vscode/settings.json").read_text()).get("idf.port")
if not port:
    parser.error("Select a serial port in ESP-IDF or pass -p PORT")
subprocess.run([sys.executable, "tools/idf.py", "-p", port, "flash"], cwd=root, check=True)
print("Finish touchscreen calibration if enabled. Waiting for the monitor's time-sync service...", flush=True)
sync_time(port)
raise SystemExit(subprocess.call([sys.executable, "tools/idf.py", "-p", port, "monitor"], cwd=root))
