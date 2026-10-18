#include "services/storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
static monitor_config_t cfg;
static nvs_handle_t nv;
static const esp_partition_t *part;
static bool healthy = true;
typedef struct __attribute__((packed)) {
    uint32_t magic, sequence;
    uint8_t payload[52];
    uint16_t reserved, crc;
} record_t;
_Static_assert(sizeof(record_t) == 64, "journal record size");
typedef struct {
    size_t base, slots, next;
    uint32_t sequence;
} journal_t;
// Keep a full erase-sector reserve: erasing 64 slots must not reduce retention
// below 7 days (10080 minute records) or the latest 500 alarms.
// Reserve space for transitions and legacy records; acknowledgments are transient.
static journal_t trends = {0, 10240, 0, 0}, alarms = {10240 * 64, 1600, 0, 0};
enum { ALARM_TRANSITION = 1, ALARM_ACKNOWLEDGE = 2 };
#define ALARM_FORMAT 0x414c4d32u
typedef struct {
    alarm_event_t event;
    uint32_t format;
    uint8_t unit, kind, reserved[2];
} stored_alarm_t;
static uint8_t legacy_alarm_unit;
static uint32_t alarm_revision;
static struct {
    uint8_t unit;
    uint32_t baseline, active, acknowledged, sequence, cutoff;
    uint64_t received_ms;
    bool seen, online;
} alarm_monitor;
static bool read_record(journal_t *j, size_t slot, record_t *r) {
    if (!part || esp_partition_read(part, j->base + slot * 64, r, 64) != ESP_OK)
        return false;
    return r->magic == 0x4d455831 && r->crc == mex_crc((uint8_t *)r, 62);
}
static void recover(journal_t *j) {
    j->sequence = 0;
    j->next = 0;
    record_t r;
    for (size_t i = 0; i < j->slots; i++)
        if (read_record(j, i, &r) && r.sequence > j->sequence) {
            j->sequence = r.sequence;
            j->next = (i + 1) % j->slots;
        }
}
static bool append(journal_t *j, const void *p, size_t n) {
    if (!part || n > 52) {
        healthy = false;
        return false;
    }
    for (size_t attempt = 0; attempt < j->slots; ++attempt) {
        record_t r;
        memset(&r, 0xff, sizeof(r));
        r.magic = 0x4d455831;
        r.sequence = j->sequence + 1;
        memcpy(r.payload, p, n);
        r.crc = mex_crc((uint8_t *)&r, 62);
        size_t offset = j->base + j->next * 64;
        // If power failed during a record, skip its slot. Only erase at a sector boundary.
        record_t raw;
        if (esp_partition_read(part, offset, &raw, 64) != ESP_OK) {
            healthy = false;
            return false;
        }
        if (j->next % 64 == 0) {
            if (esp_partition_erase_range(part, offset, 4096) != ESP_OK) {
                healthy = false;
                return false;
            }
        } else {
            bool erased = true;
            for (int i = 0; i < 64; i++)
                if (((uint8_t *)&raw)[i] != 255)
                    erased = false;
            if (!erased) {
                j->next = (j->next + 1) % j->slots;
                continue;
            }
        }
        if (esp_partition_write(part, offset, &r, 64) != ESP_OK) {
            healthy = false;
            return false;
        }
        j->sequence = r.sequence;
        j->next = (j->next + 1) % j->slots;
        return true;
    }
    healthy = false;
    return false;
}
static bool read_alarm(size_t index, stored_alarm_t *event, uint32_t *sequence) {
    record_t record;
    size_t slot = (alarms.next + alarms.slots - 1 - index) % alarms.slots;
    if (!read_record(&alarms, slot, &record))
        return false;
    memcpy(event, record.payload, sizeof(*event));
    if (event->format == UINT32_MAX) {
        // Legacy payloads were 16 bytes; the rest of the journal record was erased (0xff).
        event->unit = legacy_alarm_unit;
        event->kind = ALARM_TRANSITION;
    } else if (event->format != ALARM_FORMAT || event->unit < 1 || event->unit > 99 ||
               (event->kind != ALARM_TRANSITION && event->kind != ALARM_ACKNOWLEDGE)) {
        return false;
    }
    if (!event->event.online || !event->event.changed || (event->event.changed & ~0xffffu) ||
        (event->event.alarms & ~0xffffu))
        return false;
    *sequence = record.sequence;
    return true;
}
static void select_alarm_unit(void) {
    if (alarm_monitor.unit == cfg.unit)
        return;
    memset(&alarm_monitor, 0, sizeof(alarm_monitor));
    alarm_monitor.unit = cfg.unit;
    char key[16];
    snprintf(key, sizeof(key), "alarm_cut%02u", cfg.unit);
    size_t size = sizeof(alarm_monitor.cutoff);
    if (nvs_get_blob(nv, key, &alarm_monitor.cutoff, &size) != ESP_OK || size != sizeof(uint32_t))
        alarm_monitor.cutoff = 0;
    if (alarm_monitor.cutoff > alarms.sequence)
        alarms.sequence = alarm_monitor.cutoff;
    uint32_t settled = 0;
    bool found = false;
    for (size_t i = 0; i < alarms.slots; ++i) {
        stored_alarm_t event;
        uint32_t sequence;
        if (!read_alarm(i, &event, &sequence) || event.unit != cfg.unit)
            continue;
        if (!found) {
            alarm_monitor.baseline = event.event.alarms;
            found = true;
        }
        uint32_t bits = event.event.changed & ~settled;
        if (event.kind == ALARM_ACKNOWLEDGE)
            alarm_monitor.acknowledged |= bits;
        settled |= bits;
        if ((settled & alarm_monitor.baseline) == alarm_monitor.baseline)
            break;
    }
    alarm_monitor.acknowledged &= alarm_monitor.baseline;
    ++alarm_revision;
}
void storage_alarm_observe(const mex_snapshot_t *snapshot, uint64_t now, time_t epoch, bool clock_valid) {
    select_alarm_unit();
    bool fresh = snapshot->online && snapshot->sequence && now >= snapshot->received_ms &&
                 now - snapshot->received_ms < 3000;
    if (alarm_monitor.online != fresh) {
        alarm_monitor.online = fresh;
        ++alarm_revision;
    }
    if (!fresh || (alarm_monitor.seen && alarm_monitor.sequence == snapshot->sequence))
        return;
    uint32_t active = mex_alarms(snapshot);
    if (alarm_monitor.active != active)
        ++alarm_revision;
    alarm_monitor.active = active;
    // Acknowledgment suppresses the monitor notification only until the next
    // valid controller frame. A persisting physical alarm must notify again.
    if (alarm_monitor.acknowledged) {
        alarm_monitor.acknowledged = 0;
        ++alarm_revision;
    }
    alarm_monitor.received_ms = snapshot->received_ms;
    alarm_monitor.sequence = snapshot->sequence;
    alarm_monitor.seen = true;
    uint32_t changed = active ^ alarm_monitor.baseline;
    if (changed) {
        stored_alarm_t event = {.event = {.timestamp = clock_valid ? epoch : 0,
                                          .alarms = active,
                                          .changed = changed,
                                          .online = true},
                                .format = ALARM_FORMAT,
                                .unit = cfg.unit,
                                .kind = ALARM_TRANSITION};
        if (append(&alarms, &event, sizeof(event))) {
            alarm_monitor.baseline = active;
            ++alarm_revision;
        }
    }
}
bool storage_alarm_status(uint64_t now, uint32_t *active, uint32_t *acknowledged) {
    select_alarm_unit();
    bool fresh = alarm_monitor.online && alarm_monitor.seen && now >= alarm_monitor.received_ms &&
                 now - alarm_monitor.received_ms < 3000;
    *active = fresh ? alarm_monitor.active : 0;
    *acknowledged = fresh ? alarm_monitor.acknowledged : 0;
    return fresh;
}
uint32_t storage_alarm_revision(void) {
    return alarm_revision;
}
bool storage_alarm_clear_history(void) {
    select_alarm_unit();
    char key[16];
    snprintf(key, sizeof(key), "alarm_cut%02u", cfg.unit);
    uint32_t cutoff = alarms.sequence;
    esp_err_t error = nvs_set_blob(nv, key, &cutoff, sizeof(cutoff));
    if (error == ESP_OK)
        error = nvs_commit(nv);
    if (error != ESP_OK) {
        healthy = false;
        return false;
    }
    alarm_monitor.cutoff = cutoff;
    ++alarm_revision;
    return true;
}
bool storage_alarm_acknowledge(uint64_t now) {
    uint32_t active, acknowledged;
    if (!storage_alarm_status(now, &active, &acknowledged) || !active)
        return false;
    alarm_monitor.acknowledged = active;
    ++alarm_revision;
    return true;
}
size_t storage_alarm_history(alarm_history_entry_t *out, size_t cap) {
    select_alarm_unit();
    uint32_t cleared_at[16] = {0};
    uint16_t cleared = 0, acknowledged = 0, newer = 0;
    size_t count = 0;
    for (size_t i = 0; i < alarms.slots && count < cap && count < ALARM_HISTORY_LIMIT; ++i) {
        stored_alarm_t event;
        uint32_t sequence;
        if (!read_alarm(i, &event, &sequence) || event.unit != cfg.unit || sequence <= alarm_monitor.cutoff)
            continue;
        for (unsigned bit = 0; bit < 16 && count < cap && count < ALARM_HISTORY_LIMIT; ++bit) {
            uint16_t mask = 1u << bit;
            if (!(event.event.changed & mask))
                continue;
            if (event.kind == ALARM_ACKNOWLEDGE) {
                acknowledged |= mask;
            } else if (!(event.event.alarms & mask)) {
                cleared |= mask;
                cleared_at[bit] = event.event.timestamp;
            } else {
                out[count++] = (alarm_history_entry_t){.sequence = sequence,
                                                       .raised_at = event.event.timestamp,
                                                       .cleared_at = cleared_at[bit],
                                                       .alarm = bit,
                                                       .unit = event.unit,
                                                       .cleared = (cleared | newer) & mask,
                                                       .acknowledged = acknowledged & mask};
                newer |= mask;
                cleared &= ~mask;
                acknowledged &= ~mask;
                cleared_at[bit] = 0;
            }
        }
    }
    return count;
}
monitor_config_t *storage_config(void) {
    return &cfg;
}
bool storage_healthy(void) {
    return healthy;
}
bool storage_save(void) {
    esp_err_t e = nvs_set_blob(nv, "config", &cfg, sizeof(cfg));
    if (e == ESP_OK)
        e = nvs_commit(nv);
    if (e != ESP_OK)
        healthy = false;
    return e == ESP_OK;
}
bool storage_profile_load(uint8_t unit, void *data, size_t size) {
    if (unit < 1 || unit > 99)
        return false;
    char key[12];
    snprintf(key, sizeof(key), "desired%02u", unit);
    size_t length = size;
    return nvs_get_blob(nv, key, data, &length) == ESP_OK && length == size;
}
bool storage_profile_save(uint8_t unit, const void *data, size_t size) {
    if (unit < 1 || unit > 99)
        return false;
    char key[12];
    snprintf(key, sizeof(key), "desired%02u", unit);
    esp_err_t error = nvs_set_blob(nv, key, data, size);
    if (error == ESP_OK)
        error = nvs_commit(nv);
    if (error != ESP_OK)
        healthy = false;
    return error == ESP_OK;
}
void storage_init(void) {
    esp_err_t e = nvs_flash_init();
    ESP_ERROR_CHECK(e);
    ESP_ERROR_CHECK(nvs_open("monitor", NVS_READWRITE, &nv));
    cfg = (monitor_config_t){
        .version = 1, .quick = {230, 250, 450, 500}, .brightness = 100, .sleep_minutes = 0, .unit = 1};
    size_t n = sizeof(cfg);
    monitor_config_t read;
    if (nvs_get_blob(nv, "config", &read, &n) == ESP_OK && n == sizeof(cfg) && read.version == 1 &&
        read.unit >= 1 && read.unit <= 99 && read.brightness >= 1 && read.brightness <= 100 &&
        read.language <= 1 && read.sleep_minutes <= 10 && read.shortcut <= 2)
        cfg = read;
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "history");
    healthy = part != NULL;
    if (part) {
        recover(&trends);
        recover(&alarms);
    }
    size_t legacy_size = sizeof(legacy_alarm_unit);
    if (nvs_get_blob(nv, "alarm_legacy", &legacy_alarm_unit, &legacy_size) != ESP_OK ||
        legacy_size != sizeof(legacy_alarm_unit) || legacy_alarm_unit < 1 || legacy_alarm_unit > 99) {
        legacy_alarm_unit = cfg.unit;
        if (nvs_set_blob(nv, "alarm_legacy", &legacy_alarm_unit, sizeof(legacy_alarm_unit)) != ESP_OK ||
            nvs_commit(nv) != ESP_OK)
            healthy = false;
    }
    memset(&alarm_monitor, 0, sizeof(alarm_monitor));
    ++alarm_revision;
}
void storage_observe(const mex_snapshot_t *s, uint64_t now, time_t epoch, bool clock_valid) {
    static uint64_t last_ms, save_ms, carry[16];
    static uint32_t sequence;
    static bool was_online;
    static int64_t minute = -1;
    static int64_t sum_t, sum_h;
    static trend_point_t aggregate;
    storage_alarm_observe(s, now, epoch, clock_valid);
    if (last_ms && was_online && s->online && now - last_ms <= 2000) {
        for (int i = 0; i < 16; i++)
            if (s->coils & (1 << i)) {
                carry[i] += now - last_ms;
                cfg.runtime_s[i] += carry[i] / 1000;
                carry[i] %= 1000;
            }
    }
    last_ms = now;
    was_online = s->online;
    if (now - save_ms >= 60000) {
        storage_save();
        save_ms = now;
    }
    if (!clock_valid) {
        minute = -1;
        aggregate.samples = 0;
        return;
    }
    int64_t m = epoch / 60;
    if (m != minute) {
        if (minute >= 0 && aggregate.samples && m == minute + 1) {
            aggregate.temp = sum_t / aggregate.samples;
            aggregate.hum = sum_h / aggregate.samples;
            append(&trends, &aggregate, sizeof(aggregate));
        }
        minute = m;
        aggregate = (trend_point_t){.timestamp = m * 60,
                                    .temp_min = INT16_MAX,
                                    .hum_min = INT16_MAX,
                                    .temp_max = INT16_MIN,
                                    .hum_max = INT16_MIN};
        sum_t = sum_h = 0;
    }
    if (!s->online || s->sequence == sequence || (mex_alarms(s) & (1u << 11)))
        return;
    sequence = s->sequence;
    int t = (int16_t)s->regs[36], h = s->regs[37];
    if (t < -250 || t > 850 || h > 990)
        return;
    sum_t += t;
    sum_h += h;
    aggregate.samples++;
    if (t < aggregate.temp_min)
        aggregate.temp_min = t;
    if (t > aggregate.temp_max)
        aggregate.temp_max = t;
    if (h < aggregate.hum_min)
        aggregate.hum_min = h;
    if (h > aggregate.hum_max)
        aggregate.hum_max = h;
    aggregate.set_temp = s->regs[12];
    aggregate.set_hum = s->regs[13];
}
size_t storage_trend(trend_point_t *out, size_t cap, time_t since) {
    time_t until = time(NULL);
    if (!cap || until < since)
        return 0;
    if (since < until - 604800)
        since = until - 604800;
    size_t seconds = (until - since + 1 + cap - 1) / cap;
    if (seconds < 60)
        seconds = 60;
    size_t count = (until - since) / seconds + 1;
    if (count > cap)
        count = cap;
    memset(out, 0, cap * sizeof(*out));
    for (size_t i = 0; i < count; i++)
        out[i].timestamp = since + i * seconds;
    record_t r;
    trend_point_t p;
    bool any = false;
    for (size_t i = 0; i < trends.slots; i++)
        if (read_record(&trends, (trends.next + i) % trends.slots, &r)) {
            memcpy(&p, r.payload, sizeof(p));
            if (p.timestamp < since || p.timestamp > until || !p.samples)
                continue;
            size_t b = (p.timestamp - since) / seconds;
            if (b >= count)
                continue;
            trend_point_t *v = &out[b];
            if (!v->samples) {
                uint32_t stamp = v->timestamp;
                *v = p;
                v->timestamp = stamp;
                v->samples = 1;
            } else {
                v->temp = ((int32_t)v->temp * v->samples + p.temp) / (v->samples + 1);
                v->hum = ((int32_t)v->hum * v->samples + p.hum) / (v->samples + 1);
                v->samples++;
                if (p.temp_min < v->temp_min)
                    v->temp_min = p.temp_min;
                if (p.temp_max > v->temp_max)
                    v->temp_max = p.temp_max;
                if (p.hum_min < v->hum_min)
                    v->hum_min = p.hum_min;
                if (p.hum_max > v->hum_max)
                    v->hum_max = p.hum_max;
                v->set_temp = p.set_temp;
                v->set_hum = p.set_hum;
            }
            any = true;
        }
    return any ? count : 0;
}
size_t storage_alarms(alarm_event_t *out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < alarms.slots && n < cap && n < 500; i++) {
        stored_alarm_t event;
        uint32_t sequence;
        if (read_alarm(i, &event, &sequence) && event.kind == ALARM_TRANSITION)
            out[n++] = event.event;
    }
    return n;
}
