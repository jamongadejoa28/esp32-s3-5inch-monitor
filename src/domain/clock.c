#include "domain/clock.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void monitor_clock_set_timezone(void) {
    setenv("TZ", "KST-9", 1);
    tzset();
}
bool monitor_clock_validate(const struct tm *date, time_t *epoch) {
    if (!date || !epoch || date->tm_year < 124 || date->tm_year > 199 || date->tm_mon < 0 ||
        date->tm_mon > 11 || date->tm_mday < 1 || date->tm_mday > 31 || date->tm_hour < 0 ||
        date->tm_hour > 23 || date->tm_min < 0 || date->tm_min > 59 || date->tm_sec < 0 || date->tm_sec > 59)
        return false;
    struct tm checked = *date;
    checked.tm_isdst = 0;
    time_t value = mktime(&checked);
    if (value == (time_t)-1 || checked.tm_year != date->tm_year || checked.tm_mon != date->tm_mon ||
        checked.tm_mday != date->tm_mday || checked.tm_hour != date->tm_hour ||
        checked.tm_min != date->tm_min || checked.tm_sec != date->tm_sec)
        return false;
    *epoch = value;
    return true;
}
static int bcd(uint8_t value) {
    if ((value & 15) > 9 || (value >> 4) > 9)
        return -1;
    return (value >> 4) * 10 + (value & 15);
}
bool monitor_clock_decode_rtc(const uint8_t bytes[7], time_t *epoch) {
    if ((bytes[0] & 0x80) || bytes[4] > 6)
        return false;
    int year = bcd(bytes[6]), month = bcd(bytes[5]);
    if (year < 0 || month < 1)
        return false;
    struct tm date = {.tm_sec = bcd(bytes[0]),
                      .tm_min = bcd(bytes[1]),
                      .tm_hour = bcd(bytes[2]),
                      .tm_mday = bcd(bytes[3]),
                      .tm_mon = month - 1,
                      .tm_year = year + 100};
    return monitor_clock_validate(&date, epoch);
}
bool monitor_clock_parse_sync(const char *line, uint32_t *nonce, int64_t *epoch_ms) {
    const char prefix[] = "MEX_TIME_SET ";
    if (strncmp(line, prefix, sizeof(prefix) - 1))
        return false;
    const char *p = line + sizeof(prefix) - 1;
    if (*p < '0' || *p > '9')
        return false;
    char *end;
    errno = 0;
    unsigned long long token = strtoull(p, &end, 10);
    if (errno || token > UINT32_MAX || *end != ' ')
        return false;
    p = end + 1;
    if (*p < '0' || *p > '9')
        return false;
    errno = 0;
    unsigned long long value = strtoull(p, &end, 10);
    if (errno || *end || value > INT64_MAX)
        return false;
    time_t seconds = value / 1000;
    struct tm date;
    time_t checked;
    if (!localtime_r(&seconds, &date) || !monitor_clock_validate(&date, &checked))
        return false;
    *nonce = token;
    *epoch_ms = value;
    return true;
}
