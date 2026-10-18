#include "domain/model.h"
#include <string.h>
const mex_block_t mex_blocks[9] = {{1, 0, 8},  {1, 8, 8},  {3, 1, 3},  {3, 4, 4}, {3, 10, 12},
                                   {3, 26, 4}, {3, 35, 2}, {3, 52, 2}, {3, 58, 1}};
uint16_t mex_crc(const uint8_t *p, size_t n) {
    uint16_t c = 0xffff;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ ((c & 1) ? 0xa001 : 0);
    }
    return c;
}
size_t mex_request(uint8_t u, uint8_t f, uint16_t a, uint16_t v, uint8_t out[8]) {
    out[0] = u;
    out[1] = f;
    out[2] = a >> 8;
    out[3] = a;
    out[4] = v >> 8;
    out[5] = v;
    uint16_t c = mex_crc(out, 6);
    out[6] = c;
    out[7] = c >> 8;
    return 8;
}
int mex_response(const uint8_t q[8], const uint8_t *p, size_t n, uint16_t *v, size_t cap) {
    if (n < 5 || p[0] != q[0] || mex_crc(p, n - 2) != (uint16_t)(p[n - 2] | p[n - 1] << 8))
        return -1;
    if (p[1] == (q[1] | 0x80))
        return n == 5 && p[2] ? p[2] : -1;
    if (p[1] != q[1])
        return -2;
    if (q[1] == 5 || q[1] == 6)
        return n == 8 && !memcmp(p, q, 6) ? 0 : -3;
    unsigned count = q[4] * 256 + q[5];
    unsigned bytes = q[1] == 1 ? (count + 7) / 8 : count * 2;
    if ((q[1] != 1 && q[1] != 3) || n != bytes + 5 || p[2] != bytes || count > cap || !v)
        return -4;
    for (unsigned i = 0; i < count; i++)
        v[i] = q[1] == 1 ? ((p[3 + i / 8] >> (i % 8)) & 1) : (p[3 + 2 * i] * 256 + p[4 + 2 * i]);
    return 0;
}
bool mex_validate(const mex_command_t *c, bool online, bool running) {
    if (!online || (c->stopped_only && running))
        return false;
    if (c->fc == 5)
        return !running && c->address >= 1 && c->address <= 16 && (c->value == 0 || c->value == 1);
    if (c->fc != 6)
        return false;
    int lo = 0, hi = -1;
    switch (c->address) {
    case 40005:
        lo = 0;
        hi = 1;
        break;
    case 40012:
        lo = 50;
        hi = 500;
        break;
    case 40013:
        lo = 200;
        hi = 800;
        break;
    case 40014:
    case 40015:
        lo = 1;
        hi = 100;
        break;
    case 40016:
    case 40017:
        lo = 10;
        hi = 100;
        break;
    case 40018:
    case 40019:
        lo = 1;
        hi = 180;
        break;
    case 40021:
        lo = -100;
        hi = 100;
        break;
    case 40022:
        lo = -300;
        hi = 300;
        break;
    case 40027:
        lo = 249;
        hi = 500;
        break;
    case 40028:
        lo = 49;
        hi = 200;
        break;
    case 40029:
        lo = 599;
        hi = 900;
        break;
    case 40030:
        lo = 99;
        hi = 400;
        break;
    case 40053:
    case 40054:
        lo = 0;
        hi = 1;
        break;
    case 40059:
        lo = 1000;
        hi = 20000;
        break;
    default:
        return false;
    }
    if (running && (c->address == 40021 || c->address == 40022 || c->address >= 40027))
        return false;
    return c->value >= lo && c->value <= hi;
}
bool mex_running(const mex_snapshot_t *s) {
    return (s->regs[5] & 255) != 0;
}
uint32_t mex_alarms(const mex_snapshot_t *s) {
    uint32_t a = s->regs[3] & 0x3ff;
    const uint8_t bits[] = {2, 11, 12, 13, 14, 15};
    for (int i = 0; i < 6; i++)
        a |= ((s->regs[4] >> bits[i]) & 1) << (10 + i);
    return a;
}
uint8_t mex_stages(const mex_snapshot_t *s, unsigned kind) {
    switch (kind) {
    case 0:
        return s->coils & 1;
    case 1:
        return s->regs[6] & 15;
    case 2:
        return s->regs[7] & 31;
    case 3:
        return s->regs[8] & 3;
    case 4:
        return (s->regs[6] >> 4) & 15;
    default:
        return 0;
    }
}
int mex_proportion(int current, int setpoint, int deviation, bool reverse) {
    if (deviation <= 0)
        return 0;
    int p = (reverse ? setpoint - current : current - setpoint) * 100 / deviation;
    return p < 0 ? 0 : p > 100 ? 100 : p;
}
int mex_schedule(int64_t prev, int64_t now, int day, int tod, const uint16_t on[7], const uint16_t off[7]) {
    if (prev < 0 || now != prev + 1 || day < 0 || day > 6 || on[day] == off[day])
        return -1;
    if (tod == off[day])
        return 0;
    if (tod == on[day])
        return 1;
    return -1;
}
