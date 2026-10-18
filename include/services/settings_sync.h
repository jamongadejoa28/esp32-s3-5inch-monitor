#pragma once
#include "domain/model.h"

enum { SETTINGS_SYNC_COUNT = 8 };
// Called only from the UI task. Setpoints, deviations and delays: 40012..40019.
void settings_sync_init(void);
bool settings_sync_supported(uint16_t address);
bool settings_sync_stage(uint16_t address, int32_t value);
bool settings_sync_value(uint16_t address, const mex_snapshot_t *snapshot, int *value, bool *pending);
void settings_sync_tick(uint64_t now_ms, const mex_snapshot_t *snapshot);
unsigned settings_sync_pending(void);
uint32_t settings_sync_revision(void);
