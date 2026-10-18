#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

void monitor_clock_set_timezone(void);
bool monitor_clock_validate(const struct tm *date, time_t *epoch);
bool monitor_clock_decode_rtc(const uint8_t bytes[7], time_t *epoch);
bool monitor_clock_parse_sync(const char *line, uint32_t *nonce, int64_t *epoch_ms);
