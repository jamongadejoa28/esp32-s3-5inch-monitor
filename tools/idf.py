"""Run the supplied ESP-IDF using this project's locked Python environment."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
idf = Path(os.environ.get('IDF_PATH', '/home/hajun/esp-idf-v6.1'))
env = os.environ.copy()
env.update(IDF_PATH=str(idf), IDF_TOOLS_PATH=os.environ.get('IDF_TOOLS_PATH', '/home/hajun/dev_ws/wokwipj/.tools/espressif'),
           IDF_PYTHON_ENV_PATH=str(root/'.venv'))
# IDF's Python environment installer is deliberately not used: uv owns the lock.
if sys.argv[1:] == ['--install-tools']:
    raise SystemExit(subprocess.call([sys.executable, str(idf/'tools/idf_tools.py'),
        'install', '--targets', 'esp32s3'], env=env))
exported = subprocess.check_output([sys.executable, str(idf/'tools/idf_tools.py'),
    'export', '--format', 'key-value'], env=env, text=True)
for line in exported.splitlines():
    if '=' in line:
        key, value = line.split('=', 1)
        env[key] = value.replace('$PATH', env['PATH']).replace('${PATH}', env['PATH'])
raise SystemExit(subprocess.call([sys.executable, str(idf/'tools/idf.py'), *sys.argv[1:]], env=env, cwd=root))
