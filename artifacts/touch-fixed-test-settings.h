// SIMULATED HOST TEST ONLY - these are NOT measured board coefficients.
#pragma once
// #define BOARD_TOUCH_CALIBRATE_ON_BOOT
#define BOARD_TOUCH_USE_FIXED_CALIBRATION
#define BOARD_TOUCH_CALIBRATION_COEFFICIENTS \
    { .a = 0.000000000f, .b = -1.000000000f, .c = 1000.000000000f, \
      .d = 1.000000000f, .e = 0.000000000f, .f = -20.000000000f }
