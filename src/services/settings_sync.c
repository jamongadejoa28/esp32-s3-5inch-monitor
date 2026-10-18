#include "services/settings_sync.h"
#include "services/service.h"
#include "services/storage.h"
#include <string.h>

typedef struct {
    uint32_t version;
    uint16_t values[SETTINGS_SYNC_COUNT];
    uint8_t known, pending;
    uint16_t reserved;
} desired_profile_t;
static desired_profile_t profile;
static uint8_t target_unit;
static uint32_t revision;
static uint64_t next_send_ms, next_store_ms;
static unsigned cursor;
static bool in_flight;

bool settings_sync_supported(uint16_t address) {
    return address >= 40012 && address <= 40019;
}
static bool valid_value(unsigned index, int32_t value) {
    mex_command_t command = {.fc = 6, .address = 40012 + index, .value = value};
    return index < SETTINGS_SYNC_COUNT && mex_validate(&command, true, false);
}
static void select_unit(void) {
    uint8_t unit = service_unit();
    if (unit == target_unit)
        return;
    target_unit = unit;
    // Initial editor values; they are never sent without an explicit user save.
    profile = (desired_profile_t){.version = 2, .values = {230, 450, 10, 10, 30, 30, 1, 1}};
    desired_profile_t saved;
    if (storage_profile_load(unit, &saved, sizeof(saved)) && saved.version == 2 &&
        !(saved.pending & ~saved.known)) {
        bool valid = true;
        for (unsigned i = 0; i < SETTINGS_SYNC_COUNT; ++i)
            if ((saved.known & (1u << i)) && !valid_value(i, saved.values[i]))
                valid = false;
        if (valid)
            profile = saved;
    } else {
        struct {
            uint32_t version;
            uint16_t values[6];
            uint8_t known, pending;
            uint16_t reserved;
        } legacy;
        if (storage_profile_load(unit, &legacy, sizeof(legacy)) && legacy.version == 1 &&
            !(legacy.known & ~63u) && !(legacy.pending & ~legacy.known)) {
            bool valid = true;
            for (unsigned i = 0; i < 6; ++i)
                if ((legacy.known & (1u << i)) && !valid_value(i, legacy.values[i]))
                    valid = false;
            if (valid) {
                memcpy(profile.values, legacy.values, sizeof(legacy.values));
                profile.known = legacy.known;
                profile.pending = legacy.pending;
            }
        }
    }
    next_send_ms = next_store_ms = 0;
    cursor = 0;
    in_flight = false;
    ++revision;
}
void settings_sync_init(void) {
    target_unit = 0;
    select_unit();
}
bool settings_sync_stage(uint16_t address, int32_t value) {
    if (!settings_sync_supported(address) || !valid_value(address - 40012, value))
        return false;
    select_unit();
    desired_profile_t next = profile;
    unsigned index = address - 40012;
    next.values[index] = value;
    next.known |= 1u << index;
    next.pending |= 1u << index;
    if (!storage_profile_save(target_unit, &next, sizeof(next)))
        return false;
    profile = next;
    next_send_ms = 0;
    ++revision;
    return true;
}
bool settings_sync_value(uint16_t address, const mex_snapshot_t *snapshot, int *value, bool *pending) {
    if (!settings_sync_supported(address))
        return false;
    select_unit();
    unsigned i = address - 40012;
    *pending = (profile.pending & (1u << i)) != 0;
    *value = !*pending && snapshot->online ? snapshot->regs[address - 40000] : profile.values[i];
    return true;
}
unsigned settings_sync_pending(void) {
    select_unit();
    unsigned count = 0;
    for (unsigned i = 0; i < SETTINGS_SYNC_COUNT; ++i)
        count += (profile.pending >> i) & 1;
    return count;
}
uint32_t settings_sync_revision(void) {
    return revision;
}
void settings_sync_tick(uint64_t now, const mex_snapshot_t *snapshot) {
    select_unit();
    command_state_t result = service_result();
    if (result == CMD_PENDING)
        return;
    mex_snapshot_t completed;
    if (in_flight) {
        // Read after completion, so even a caller's stale snapshot cannot clear a newer edit.
        service_snapshot(&completed);
        snapshot = &completed;
        in_flight = false;
    }
    if (!snapshot->online || now < snapshot->received_ms || now - snapshot->received_ms >= 3000)
        return;
    desired_profile_t next = profile;
    for (unsigned i = 0; i < SETTINGS_SYNC_COUNT; ++i) {
        uint16_t actual = snapshot->regs[12 + i];
        if (!valid_value(i, actual))
            continue;
        if (next.pending & (1u << i)) {
            // The latest desired value, not a write echo, must match the polled controller.
            if (actual == next.values[i])
                next.pending &= ~(1u << i);
        } else {
            next.values[i] = actual;
            next.known |= 1u << i;
        }
    }
    if (next.pending != profile.pending) {
        if (now >= next_store_ms) {
            if (storage_profile_save(target_unit, &next, sizeof(next))) {
                profile = next;
                next_send_ms = now;
                ++revision;
            } else {
                next_store_ms = now + 5000;
            }
        }
    } else if (memcmp(&next, &profile, sizeof(next))) {
        profile = next;
        ++revision;
    }
    if (!profile.pending || now < next_send_ms || service_result() == CMD_PENDING)
        return;
    for (unsigned attempt = 0; attempt < SETTINGS_SYNC_COUNT; ++attempt) {
        unsigned i = cursor++ % SETTINGS_SYNC_COUNT;
        if (!(profile.pending & (1u << i)))
            continue;
        if (snapshot->regs[12 + i] != profile.values[i]) {
            // A setting is idempotent: retry after a timeout/rejection while preserving it in NVS.
            in_flight = service_send(6, 40012 + i, profile.values[i], false);
            next_send_ms = now + 5000;
            break;
        }
    }
}
