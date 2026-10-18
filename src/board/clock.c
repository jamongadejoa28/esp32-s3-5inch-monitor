#include "board/clock.h"
#include "board/board.h"
#include "domain/clock.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static const char *TAG = "clock";
static i2c_master_dev_handle_t rtc_device;
static nvs_handle_t preferences;
static bool valid, provisioned;
static uint32_t nonce;
static uint64_t issued_ms;

static uint8_t bcd(int value) {
    return value / 10 * 16 + value % 10;
}
static bool apply_time(int64_t milliseconds, const char *source) {
    time_t epoch = milliseconds / 1000, checked;
    struct tm date;
    if (milliseconds < 0 || !localtime_r(&epoch, &date) || !monitor_clock_validate(&date, &checked))
        return false;
    struct timeval now = {.tv_sec = epoch, .tv_usec = (milliseconds % 1000) * 1000};
    if (settimeofday(&now, NULL))
        return false;
    valid = true;
    uint8_t bytes[] = {4,
                       bcd(date.tm_sec),
                       bcd(date.tm_min),
                       bcd(date.tm_hour),
                       bcd(date.tm_mday),
                       bcd(date.tm_wday),
                       bcd(date.tm_mon + 1),
                       bcd(date.tm_year - 100)};
    bool rtc_written = i2c_master_transmit(rtc_device, bytes, sizeof(bytes), 50) == ESP_OK;
    if (!rtc_written && provisioned) {
        // A failed update can leave an old but plausible RTC date. Do not restore it on reboot.
        uint32_t marker = 0;
        esp_err_t error = nvs_set_blob(preferences, "kst_format", &marker, sizeof(marker));
        if (error == ESP_OK)
            error = nvs_commit(preferences);
        if (error != ESP_OK)
            ESP_LOGW(TAG, "Could not invalidate the old RTC commissioning marker.");
        provisioned = false;
    }
    if (rtc_written && !provisioned) {
        uint32_t marker = 0x4b535431; // KST1; old factory/default dates are not trusted.
        provisioned = nvs_set_blob(preferences, "kst_format", &marker, sizeof(marker)) == ESP_OK &&
                      nvs_commit(preferences) == ESP_OK;
    }
    char stamp[40];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S KST", &date);
    ESP_LOGI(TAG, "%s synchronized: %s (RTC written=%d, commissioned=%d)", source, stamp, rtc_written,
             provisioned);
    if (!rtc_written || !provisioned)
        ESP_LOGW(TAG, "System time is valid for this session; RTC persistence could not be confirmed.");
    return true;
}
bool board_clock_set(struct tm *date) {
    time_t epoch;
    return monitor_clock_validate(date, &epoch) && apply_time((int64_t)epoch * 1000, "manual");
}
bool board_clock_valid(void) {
    return valid;
}
void board_clock_init(i2c_master_dev_handle_t device) {
    rtc_device = device;
    valid = false;
    monitor_clock_set_timezone();
    ESP_ERROR_CHECK(nvs_open("clock", NVS_READWRITE, &preferences));
    uint32_t marker = 0;
    size_t length = sizeof(marker);
    provisioned = nvs_get_blob(preferences, "kst_format", &marker, &length) == ESP_OK &&
                  length == sizeof(marker) && marker == 0x4b535431;
    uint8_t reg = 4, bytes[7];
    time_t epoch;
    if (provisioned && i2c_master_transmit_receive(device, &reg, 1, bytes, sizeof(bytes), 100) == ESP_OK &&
        monitor_clock_decode_rtc(bytes, &epoch)) {
        struct timeval now = {.tv_sec = epoch};
        valid = settimeofday(&now, NULL) == 0;
    }
    if (valid) {
        ESP_LOGI(TAG, "Restored commissioned RTC time (KST, UTC+09:00).");
    } else {
        // Never invent a date from the build timestamp or an uncommissioned factory RTC.
        ESP_LOGW(TAG, "Time not synchronized. Use the PC time-sync task or Settings > Clock.");
    }
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = {.tx_buffer_size = 1024, .rx_buffer_size = 256};
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    }
}
static void request(const char *line) {
    uint64_t now = esp_timer_get_time() / 1000;
    if (!strcmp(line, "MEX_TIME_GET")) {
        nonce = (uint32_t)esp_timer_get_time() | 1u;
        issued_ms = now;
        printf("\nMEX_TIME_READY %" PRIu32 "\n", nonce);
        fflush(stdout);
        return;
    }
    uint32_t token;
    int64_t epoch_ms;
    if (!monitor_clock_parse_sync(line, &token, &epoch_ms) || !nonce || token != nonce ||
        now - issued_ms > 10000) {
        printf("\nMEX_TIME_ERROR invalid_or_expired_request\n");
    } else {
        nonce = 0;
        printf("\nMEX_TIME_%s\n", apply_time(epoch_ms, "USB PC") ? "OK" : "ERROR set_failed");
    }
    fflush(stdout);
}
void board_clock_poll(void) {
    static char line[96];
    static size_t used;
    static bool overflow;
    char bytes[64];
    int count = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), 0);
    for (int i = 0; i < count; ++i) {
        char c = bytes[i];
        if (c == '\r')
            continue;
        if (c == '\n') {
            if (!overflow && used) {
                line[used] = 0;
                request(line);
            }
            used = 0;
            overflow = false;
        } else if (used < sizeof(line) - 1 && !overflow) {
            line[used++] = c;
        } else {
            overflow = true;
        }
    }
}
