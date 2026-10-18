#include "services/settings_sync.h"
#include "services/storage.h"
#include "services/service.h"
#include <assert.h>
#include <stdio.h>

static uint8_t unit = 1;
static command_state_t result;
static uint16_t sent_address, sent_value;
static unsigned writes;
static bool allow_send = true;
static const mex_snapshot_t *controller;
void service_snapshot(mex_snapshot_t *out) {
    *out = *controller;
}
extern void host_nvs_fail(bool fail);
uint8_t service_unit(void) {
    return unit;
}
command_state_t service_result(void) {
    return result;
}
bool service_send(uint8_t fc, uint16_t address, int32_t value, bool stopped) {
    assert(fc == 6 && settings_sync_supported(address) && !stopped);
    if (!allow_send) {
        result = CMD_REJECTED;
        return false;
    }
    sent_address = address;
    sent_value = value;
    result = CMD_PENDING;
    ++writes;
    return true;
}
int main(void) {
    storage_init();
    settings_sync_init();
    mex_snapshot_t snapshot = {0};
    controller = &snapshot;
    int value;
    bool pending;
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 230 && !pending);
    assert(!settings_sync_stage(40005, 1)); // never queues power or manual outputs
    assert(!settings_sync_stage(40012, 501));
    assert(!settings_sync_stage(40016, 9));
    assert(!settings_sync_stage(40018, 0));
    assert(!settings_sync_stage(40019, 181));
    const int wanted[] = {250, 500, 15, 20, 35, 40, 12, 18};
    for (int i = 0; i < 8; ++i)
        assert(settings_sync_stage(40012 + i, wanted[i]));
    assert(settings_sync_stage(40012, 251)); // newest offline edit replaces the previous one
    settings_sync_tick(100, &snapshot);
    assert(writes == 0 && settings_sync_pending() == 8);
    settings_sync_init(); // process restart, NVS retained
    assert(settings_sync_pending() == 8);
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 251 && pending);
    unit = 2;
    settings_sync_tick(200, &snapshot);
    assert(settings_sync_pending() == 0); // another address must never inherit unit 1 settings
    assert(settings_sync_stage(40012, 300));
    unit = 1;
    settings_sync_tick(300, &snapshot);
    assert(settings_sync_pending() == 8);
    snapshot.online = true;
    snapshot.received_ms = 0;
    settings_sync_tick(5000, &snapshot);
    assert(writes == 0); // stale online flag is insufficient
    snapshot.received_ms = 5100;
    settings_sync_tick(5100, &snapshot);
    assert(writes == 1 && sent_address == 40012 && sent_value == 251);
    result = CMD_APPLIED;
    snapshot.received_ms = 5200;
    settings_sync_tick(5200, &snapshot);
    assert(settings_sync_pending() == 8);    // write echo alone does not clear anything
    assert(settings_sync_stage(40012, 252)); // edited again while an old value is in flight
    snapshot.regs[12] = 251;
    settings_sync_tick(5300, &snapshot);
    assert(settings_sync_pending() == 8);
    result = CMD_IDLE;
    for (uint64_t now = 10000; settings_sync_pending() && now < 100000; now += 1000) {
        snapshot.received_ms = now;
        settings_sync_tick(now, &snapshot);
        if (result == CMD_PENDING) {
            snapshot.regs[sent_address - 40000] = sent_value;
            result = CMD_UNCERTAIN; // lost write ack, but subsequent readback is authoritative
        }
    }
    assert(settings_sync_pending() == 0 && snapshot.regs[12] == 252);
    for (int i = 1; i < 8; ++i)
        assert(snapshot.regs[12 + i] == wanted[i]);
    settings_sync_init();
    assert(settings_sync_pending() == 0);
    snapshot.online = false;
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 252 && !pending);
    host_nvs_fail(true);
    assert(!settings_sync_stage(40012, 260));
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 252 && !pending);
    host_nvs_fail(false);
    unit = 2;
    settings_sync_tick(110000, &snapshot);
    assert(settings_sync_pending() == 1);
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 300 && pending);
    allow_send = false;
    snapshot.online = true;
    snapshot.received_ms = 110100;
    settings_sync_tick(110100, &snapshot);
    assert(settings_sync_pending() == 1);
    allow_send = true;
    snapshot.received_ms = 116000;
    settings_sync_tick(116000, &snapshot);
    assert(sent_value == 300);
    snapshot.regs[12] = 300;
    result = CMD_UNCERTAIN;
    host_nvs_fail(true);
    snapshot.received_ms = 116100;
    settings_sync_tick(116100, &snapshot);
    assert(settings_sync_pending() == 1); // failed persistence must not lose the pending flag
    host_nvs_fail(false);
    snapshot.received_ms = 122000;
    settings_sync_tick(122000, &snapshot);
    assert(settings_sync_pending() == 0);
    // A stale matching snapshot must not erase an edit made during an older write.
    snapshot.online = true;
    snapshot.received_ms = 123000;
    result = CMD_IDLE;
    assert(settings_sync_stage(40012, 301));
    settings_sync_tick(123000, &snapshot);
    assert(result == CMD_PENDING);
    assert(settings_sync_stage(40012, 300));
    mex_snapshot_t stale = snapshot; // still shows 300 before the older 301 write finishes
    settings_sync_tick(123010, &stale);
    assert(settings_sync_pending() == 1);
    snapshot.regs[12] = 301;
    snapshot.received_ms = 123100;
    result = CMD_APPLIED;
    settings_sync_tick(123100, &stale);
    assert(settings_sync_pending() == 1 && sent_value == 300);
    struct {
        uint32_t version;
        uint16_t values[6];
        uint8_t known, pending;
        uint16_t reserved;
    } legacy = {1, {240, 460, 10, 20, 30, 40}, 63, 33, 0};
    assert(storage_profile_save(3, &legacy, sizeof(legacy)));
    unit = 3;
    settings_sync_init();
    snapshot.online = false;
    assert(settings_sync_pending() == 2);
    assert(settings_sync_value(40012, &snapshot, &value, &pending) && value == 240 && pending);
    assert(settings_sync_value(40019, &snapshot, &value, &pending) && value == 1 && !pending);
    assert(settings_sync_stage(40019, 180));
    settings_sync_init();
    assert(settings_sync_pending() == 3);
    puts("Offline settings: eight values, NVS restart, coalescing, unit isolation, readback, stale link, "
         "retry "
         "and storage failure PASS");
}
