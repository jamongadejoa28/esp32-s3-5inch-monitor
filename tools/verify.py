"""Reproducible host and IDF checks. Run: uv run --locked python tools/verify.py"""
from pathlib import Path
import subprocess
import sys
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1]
def run(*cmd):subprocess.run(cmd,cwd=root,check=True)
run(sys.executable,'-m','pytest','-q')
run('cmake','-S','tests/host','-B','build-host','-G','Ninja')
run('cmake','--build','build-host','-j','8')
run('./build-host/storage_check')
run('./build-host/alarm_check')
run('./build-host/settings_sync_check')
run('./build-host/clock_check')
run('./build-host/touch_check')
run('./build-host/touch_calibration_check')
run('./build-host/touch_fixed_check')
run('./build-host/monitor_preview')
for p in (root/'artifacts').glob('*.ppm'):
    Image.open(p).save(p.with_suffix('.png'))
a=Image.open(root/'artifacts/animation-a.png');b=Image.open(root/'artifacts/animation-b.png')
assert ImageChops.difference(a,b).getbbox(), 'Animation frames must differ'
frames=[Image.open(root/f'artifacts/motion-{i:02}.png') for i in range(12)]
frames[0].save(root/'artifacts/dashboard-animation.gif',save_all=True,append_images=frames[1:],duration=100,loop=0)
run(sys.executable,'tools/idf.py','build')
print('All LVGL 9.6.0 host tests and ESP-IDF v6.1 build passed. Hardware has NOT been tested.')
