#pragma once
#include "domain/model.h"
typedef enum { CMD_IDLE, CMD_PENDING, CMD_APPLIED, CMD_REJECTED, CMD_UNCERTAIN } command_state_t;
void service_init(void);
void service_snapshot(mex_snapshot_t *out);
bool service_send(uint8_t fc, uint16_t address, int32_t raw, bool stopped_only);
command_state_t service_result(void);
void service_set_unit(uint8_t unit);
uint8_t service_unit(void);
// ESP-IDF high-water mark: minimum unused bytes since task creation.
uint32_t service_stack_free_min(void);
