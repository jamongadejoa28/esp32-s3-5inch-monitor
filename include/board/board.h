#pragma once
#include <stdbool.h>
#include <time.h>
#include "board/touch_calibration.h"
void board_init(void);
void board_display_on(bool on);
bool board_clock_set(struct tm *date);
bool board_clock_valid(void);
touch_raw_sample_t board_touch_read_raw(void);
bool board_touch_apply_calibration(const touch_transform_t *transform);
void board_touch_input_enable(bool enabled);
