#include "board/board.h"
#include "board/clock.h"
#include "domain/clock.h"
#include "driver/usb_serial_jtag.h"
#include "nvs_flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static uint8_t rtc[7] = {0, 0x28, 9, 2, 1, 2, 0x26};
static bool driver, fail_write;
static uint64_t monotonic_us = 1000000;
static struct timeval applied;
static char input[512];
static size_t offset;
int64_t esp_timer_get_time(void) {
    return monotonic_us;
}
bool usb_serial_jtag_is_driver_installed(void) {
    return driver;
}
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t *config) {
    assert(config->rx_buffer_size >= 96);
    driver = true;
    return ESP_OK;
}
int usb_serial_jtag_read_bytes(void *bytes, uint32_t size, uint32_t wait) {
    assert(wait == 0);
    size_t n = strlen(input) - offset;
    if (n > size)
        n = size;
    memcpy(bytes, input + offset, n);
    offset += n;
    return n;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device, const uint8_t *bytes, size_t size, int wait) {
    (void)device;
    (void)wait;
    assert(size == 8 && bytes[0] == 4);
    if (fail_write)
        return -1;
    memcpy(rtc, bytes + 1, 7);
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t device, const uint8_t *reg, size_t reg_size,
                                      uint8_t *bytes, size_t size, int wait) {
    (void)device;
    (void)wait;
    assert(reg_size == 1 && *reg == 4 && size == 7);
    memcpy(bytes, rtc, 7);
    return ESP_OK;
}
int host_settimeofday(const struct timeval *value, const struct timezone *zone) {
    (void)zone;
    applied = *value;
    return 0;
}
static void command(const char *value) {
    snprintf(input, sizeof(input), "%s", value);
    offset = 0;
    while (offset < strlen(input))
        board_clock_poll();
}
int main(void) {
    nvs_flash_init();
    board_clock_init(NULL);
    assert(driver && !board_clock_valid()); // plausible factory RTC is not commissioned
    time_t epoch;
    assert(monitor_clock_decode_rtc(rtc, &epoch));
    rtc[0] |= 0x80;
    assert(!monitor_clock_decode_rtc(rtc, &epoch));
    rtc[0] = 0x1f;
    assert(!monitor_clock_decode_rtc(rtc, &epoch));
    struct tm date = {.tm_year = 126, .tm_mon = 9, .tm_mday = 18, .tm_hour = 23, .tm_min = 59, .tm_sec = 58};
    assert(monitor_clock_validate(&date, &epoch));
    struct tm utc;
    gmtime_r(&epoch, &utc);
    assert(utc.tm_hour == 14); // KST is exactly UTC+9
    uint32_t token;
    int64_t milliseconds;
    assert(!monitor_clock_parse_sync("MEX_TIME_SET -1 1792335598000", &token, &milliseconds));
    assert(!monitor_clock_parse_sync("MEX_TIME_SET 4294967296 1792335598000", &token, &milliseconds));
    assert(!monitor_clock_parse_sync("MEX_TIME_SET 1 1792335598000junk", &token, &milliseconds));
    assert(!monitor_clock_parse_sync("MEX_TIME_SET 1 0", &token, &milliseconds));
    assert(!monitor_clock_parse_sync("MEX_TIME_SET 1 999999999999999999999999", &token, &milliseconds));
    command("MEX_TIME_GET\n");
    char line[128];
    snprintf(line, sizeof(line), "MEX_TIME_SET 1 %lld\n", (long long)epoch * 1000 + 123);
    command(line);
    assert(!board_clock_valid()); // wrong nonce
    snprintf(line, sizeof(line), "MEX_TIME_SET 1000001 %lld\n", (long long)epoch * 1000 + 123);
    command(line);
    assert(board_clock_valid() && applied.tv_sec == epoch && applied.tv_usec == 123000);
    assert(rtc[2] == 0x23 && rtc[3] == 0x18 && rtc[5] == 0x10 && rtc[6] == 0x26);
    board_clock_init(NULL);
    assert(board_clock_valid()); // commissioned RTC survives CPU reset while powered
    rtc[0] |= 0x80;
    board_clock_init(NULL);
    assert(!board_clock_valid()); // batteryless power loss invalidates time
    command("MEX_TIME_GET\n");
    monotonic_us = 12000000;
    command(line);
    assert(!board_clock_valid()); // expired handshake
    date.tm_mon = 1;
    date.tm_mday = 30;
    assert(!board_clock_set(&date));
    date.tm_year = 124;
    date.tm_mday = 29;
    fail_write = true;
    assert(board_clock_set(&date) && board_clock_valid()); // system clock still usable if RTC write fails
    fail_write = false;
    board_clock_init(NULL);
    assert(!board_clock_valid()); // failed RTC update must not restore an older commissioned value
    char overflow[240];
    memset(overflow, 'x', 200);
    snprintf(overflow + 200, sizeof(overflow) - 200, "\nMEX_TIME_GET\n");
    command(overflow); // oversized line discarded, parser recovers at the following newline
    puts("KST clock: validation, OS flag, commissioning, USB handshake, expiry, overflow and RTC failure "
         "PASS");
}
