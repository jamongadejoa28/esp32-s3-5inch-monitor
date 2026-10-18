#include "services/storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void host_corrupt(size_t);
int main(void) {
    storage_init();
    assert(storage_healthy());
    storage_config()->quick[0] = 234;
    assert(storage_save());
    storage_config()->quick[0] = 999;
    storage_init();
    assert(storage_config()->quick[0] == 234);
    mex_snapshot_t s = {.online = true, .sequence = 1};
    s.regs[12] = 230;
    s.regs[13] = 450;
    s.regs[36] = 240;
    s.regs[37] = 500;
    s.coils = 1;
    time_t end = time(NULL), base = (end / 60) * 60 - 11000 * 60;
    uint64_t now = 1000;
    for (int i = 0; i < 11000; i++) {
        s.sequence++;
        s.regs[36] = 200 + i % 100;
        s.received_ms = now;
        storage_observe(&s, now, base + i * 60, true);
        now += 1000;
        s.received_ms = now;
        storage_observe(&s, now, base + i * 60 + 1, true);
        now += 1000;
    }
    trend_point_t points[180];
    size_t n = storage_trend(points, 180, end - 604800);
    assert(n > 0 && n <= 180);
    int count = 0;
    for (size_t i = 0; i < n; i++) {
        assert(points[i].timestamp >= end - 604800);
        if (points[i].samples) {
            count++;
            assert(points[i].temp_min <= points[i].temp && points[i].temp <= points[i].temp_max);
        }
    }
    assert(count > 170);
    for (int i = 0; i < 620; i++) {
        s.sequence++;
        s.regs[3] = i % 2 ? 32 : 0;
        s.received_ms = now;
        storage_observe(&s, now, base + 11000 * 60 + i, true);
        now += 1000;
    }
    alarm_event_t events[500];
    assert(storage_alarms(events, 500) == 500);
    uint64_t runtime = storage_config()->runtime_s[0];
    s.online = false;
    for (int i = 0; i < 100; i++) {
        s.received_ms = now;
        storage_observe(&s, now, end + i, true);
        now += 1000;
    }
    assert(storage_config()->runtime_s[0] == runtime);
    // Corruption cannot become a valid event and recovery must tolerate erased/torn tails.
    host_corrupt(10240 * 64 + 4);
    storage_init();
    assert(storage_alarms(events, 500) > 450);
    puts("Journal wrap, bounded retention, CRC recovery, NVS restore and offline runtime checks passed");
}
