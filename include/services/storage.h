#pragma once
#include "domain/model.h"
typedef struct {
    uint32_t version;
    int quick[4];
    uint8_t mode[4], brightness, sleep_minutes, language, unit, shortcut;
    uint16_t schedule_on[7], schedule_off[7];
    uint64_t runtime_s[16];
} monitor_config_t;
typedef struct {
    uint32_t timestamp;
    int16_t temp, hum, temp_min, temp_max, hum_min, hum_max, set_temp, set_hum;
    uint16_t samples;
} trend_point_t;
typedef struct {
    uint32_t timestamp;
    uint32_t alarms, changed;
    bool online;
} alarm_event_t;
enum { ALARM_HISTORY_LIMIT = 500, ALARM_PAGE_SIZE = 50 };
typedef struct {
    uint32_t sequence, raised_at, cleared_at;
    uint8_t alarm, unit;
    bool cleared, acknowledged;
} alarm_history_entry_t;
void storage_init(void);
monitor_config_t *storage_config(void);
bool storage_save(void);
void storage_observe(const mex_snapshot_t *s, uint64_t now_ms, time_t epoch, bool clock_valid);
size_t storage_trend(trend_point_t *out, size_t cap, time_t since);
size_t storage_alarms(alarm_event_t *out, size_t cap);
size_t storage_alarm_history(alarm_history_entry_t *out, size_t cap);
void storage_alarm_observe(const mex_snapshot_t *snapshot, uint64_t now_ms, time_t epoch, bool clock_valid);
bool storage_alarm_status(uint64_t now_ms, uint32_t *active, uint32_t *acknowledged);
bool storage_alarm_clear_history(void);
bool storage_alarm_acknowledge(uint64_t now_ms);
uint32_t storage_alarm_revision(void);
bool storage_healthy(void);
// Separate NVS keys preserve the existing monitor configuration format.
bool storage_profile_load(uint8_t unit, void *data, size_t size);
bool storage_profile_save(uint8_t unit, const void *data, size_t size);
