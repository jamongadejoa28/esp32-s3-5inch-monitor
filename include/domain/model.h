#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
typedef struct {
    uint16_t regs[60], coils;
    uint64_t received_ms;
    uint32_t sequence;
    bool online;
} mex_snapshot_t;
typedef struct {
    uint8_t fc;
    uint16_t address;
    int32_t value;
    bool stopped_only;
    uint32_t id;
} mex_command_t;
typedef struct {
    uint8_t fc;
    uint16_t start, count;
} mex_block_t;
extern const mex_block_t mex_blocks[9];
uint16_t mex_crc(const uint8_t *p, size_t n);
size_t mex_request(uint8_t unit, uint8_t fc, uint16_t address, uint16_t value, uint8_t out[8]);
// 0 success, positive Modbus exception, negative malformed/mismatched response.
int mex_response(const uint8_t req[8], const uint8_t *p, size_t n, uint16_t *values, size_t cap);
bool mex_validate(const mex_command_t *cmd, bool online, bool running);
uint32_t mex_alarms(const mex_snapshot_t *s);
uint8_t mex_stages(const mex_snapshot_t *s, unsigned kind);
bool mex_running(const mex_snapshot_t *s);
int mex_proportion(int current, int setpoint, int deviation, bool reverse);
// No replay: only a continuously observed, immediately adjacent minute can trigger.
int mex_schedule(int64_t previous_minute, int64_t minute, int weekday, int minute_of_day,
                 const uint16_t on[7], const uint16_t off[7]);
