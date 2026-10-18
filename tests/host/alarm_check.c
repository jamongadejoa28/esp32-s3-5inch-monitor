#include "services/storage.h"
#include "esp_partition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void host_nvs_fail(bool fail);
static mex_snapshot_t snapshot;
static uint64_t now = 1000;
static time_t epoch = 1792350000;
static alarm_history_entry_t entries[500];
static void frame(uint16_t alarms) {
    snapshot.online = true;
    snapshot.sequence++;
    snapshot.received_ms = ++now;
    snapshot.regs[3] = alarms;
    storage_alarm_observe(&snapshot, now, ++epoch, true);
}
static size_t count(void) {
    return storage_alarm_history(entries, 500);
}
static void status(uint32_t expected, uint32_t ack) {
    uint32_t active, acknowledged;
    assert(storage_alarm_status(now, &active, &acknowledged));
    assert(active == expected && acknowledged == ack);
}
int main(void) {
    storage_init();
    frame(0);
    assert(count() == 0);
    frame(32);
    assert(count() == 1 && !entries[0].cleared);
    uint32_t raised = entries[0].raised_at;
    status(32, 0);
    assert(storage_alarm_acknowledge(now));
    status(32, 32);
    storage_alarm_observe(&snapshot, now, epoch, true);
    status(32, 32); // UI ticks cannot substitute for another controller response
    frame(32);
    status(32, 0);
    assert(count() == 1);
    for (int i = 0; i < 20; ++i) {
        assert(storage_alarm_acknowledge(now));
        frame(32);
    }
    assert(count() == 1); // repeated acknowledgments never fabricate incidents
    snapshot.online = false;
    snapshot.regs[3] = 0;
    storage_alarm_observe(&snapshot, ++now, ++epoch, true);
    assert(count() == 1 && !entries[0].cleared); // disconnect isn't normal recovery
    assert(!storage_alarm_acknowledge(now));
    frame(32);
    assert(count() == 1);
    snapshot.sequence++;
    snapshot.regs[3] = 0;
    storage_alarm_observe(&snapshot, now + 4000, epoch, true);
    assert(count() == 1 && !entries[0].cleared); // stale online snapshot cannot clear
    frame(0);
    assert(count() == 1 && entries[0].cleared && entries[0].raised_at == raised &&
           entries[0].cleared_at == epoch);
    frame(32);
    assert(count() == 2 && !entries[0].cleared && entries[1].cleared);
    host_nvs_fail(true);
    assert(!storage_alarm_clear_history() && count() == 2);
    host_nvs_fail(false);
    assert(storage_alarm_clear_history() && count() == 0);
    status(32, 0);
    frame(32);
    assert(count() == 0); // ongoing alarm doesn't recreate deleted history
    storage_init();
    frame(32);
    assert(count() == 0);
    status(32, 0);
    frame(0);
    assert(count() == 0);
    frame(32);
    assert(count() == 1);
    storage_config()->unit = 2;
    frame(64);
    assert(count() == 1 && entries[0].unit == 2 && entries[0].alarm == 6);
    assert(storage_alarm_clear_history());
    storage_config()->unit = 1;
    frame(32);
    assert(count() == 1 && entries[0].unit == 1);
    frame(0);
    for (int i = 0; i < 900; ++i) {
        frame(32);
        frame(0);
    }
    assert(count() == 500);
    for (size_t i = 0; i < 500; ++i)
        assert(entries[i].cleared && entries[i].cleared_at >= entries[i].raised_at);
    storage_init();
    frame(0);
    assert(count() == 500);
    // Seed old-format records in an unused legacy sector, including old link events.
    const esp_partition_t *part = esp_partition_find_first(0, 0, "history");
    assert(esp_partition_erase_range(part, 10240 * 64, 4096) == 0);
    uint8_t record[64];
    for (unsigned i = 0; i < 4; ++i) {
        memset(record, 255, sizeof(record));
        uint32_t magic = 0x4d455831, sequence = 100000 + i;
        memcpy(record, &magic, 4);
        memcpy(record + 4, &sequence, 4);
        alarm_event_t old = {.timestamp = epoch + i,
                             .alarms = i == 1 ? 32 : 0,
                             .changed = i == 1 || i == 2 ? 32 : 0,
                             .online = i != 3};
        memcpy(record + 8, &old, sizeof(old));
        uint16_t crc = mex_crc(record, 62);
        memcpy(record + 62, &crc, 2);
        assert(esp_partition_write(part, 10240 * 64 + i * 64, record, 64) == 0);
    }
    storage_init();
    assert(count() == 500 && entries[0].cleared && entries[0].raised_at == epoch + 1 &&
           entries[0].cleared_at == epoch + 2);
    puts("Real alarm transitions, recovery, transient acknowledgment/re-detection, deletion persistence, "
         "unit isolation, 500 incidents and legacy filtering PASS");
}
