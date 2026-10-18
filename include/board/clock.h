#pragma once
#include "driver/i2c_master.h"

// RTC stores Korean local civil time; NVS marks this firmware's commissioned format.
void board_clock_init(i2c_master_dev_handle_t rtc);
void board_clock_poll(void);
