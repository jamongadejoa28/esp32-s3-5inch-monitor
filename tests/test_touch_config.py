"""Prevent ambiguous commissioning settings from silently selecting a mode."""
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("settings,message", [
    ("", "Select exactly one touch mode"),
    ("#define BOARD_TOUCH_CALIBRATE_ON_BOOT\n#define BOARD_TOUCH_USE_FIXED_CALIBRATION\n",
     "Select exactly one touch mode"),
    ("#define BOARD_TOUCH_USE_FIXED_CALIBRATION\n", "Paste the measured"),
])
def test_invalid_mode_selection(tmp_path, settings, message):
    (tmp_path / "board").mkdir()
    (tmp_path / "board/touch_settings.h").write_text(settings)
    includes = [tmp_path, ROOT / "tests/host/include", ROOT / "include",
                ROOT / "managed_components/lvgl__lvgl",
                ROOT / "managed_components/lvgl__lvgl/include"]
    args = ["cc", "-E", "-DLV_CONF_INCLUDE_SIMPLE=1"]
    for folder in includes:
        args.extend(["-I", str(folder)])
    args.extend([str(ROOT / "src/board/touch_calibration.c"), "-o", "/dev/null"])
    result = subprocess.run(args, text=True, capture_output=True, check=False)
    assert result.returncode != 0
    assert message in result.stderr
