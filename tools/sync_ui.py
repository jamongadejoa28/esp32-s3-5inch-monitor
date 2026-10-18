"""Copy EEZ's ui folder intact after Build; never rewrites generated files."""
from pathlib import Path
import shutil
import argparse
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--eez-dir',type=Path,default=Path('/mnt/c/Users/hajun/eez-projects/esp32-s3-5inch-monitor'));a=p.parse_args()
source = a.eez_dir / 'src/ui'
if not (source / 'screens.c').is_file() or not (source / 'screens.h').is_file():
    raise SystemExit('Build the target EEZ project before syncing: missing screens.c/screens.h')
target = root / 'main/ui'
target.mkdir(parents=True, exist_ok=True)
# Remove only obsolete generated code, avoiding duplicate symbols after EEZ renames.
for stale in target.rglob('*'):
    if stale.is_file() and stale.suffix in ('.c', '.h'):
        if not (source / stale.relative_to(target)).is_file():
            stale.unlink()
shutil.copytree(source, target, dirs_exist_ok=True)
shutil.copy2(a.eez_dir/'esp32-s3-5inch-monitor.eez-project',root/'esp32-s3-5inch-monitor.eez-project')
print('EEZ project and intact ui folder synchronized to main/ui')
