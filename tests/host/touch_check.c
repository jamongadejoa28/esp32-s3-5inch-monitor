#include "board/touch_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static const touch_point_t targets[5] = {{64, 64}, {735, 64}, {735, 415}, {64, 415}, {400, 240}};
static void math_checks(void) {
    touch_transform_t t;
    assert(touch_transform_fit(targets, targets, 4, &t));
    assert(fabsf(t.a - 1) < 0.001 && fabsf(t.e - 1) < 0.001);
    touch_point_t raw[5];
    // Offset + swapped and reversed axes (raw Y exceeds the display height).
    for (int i = 0; i < 5; ++i)
        raw[i] = (touch_point_t){targets[i].y + 20, 1000 - targets[i].x};
    assert(touch_transform_fit(raw, targets, 4, &t));
    for (int i = 0; i < 5; ++i)
        assert(touch_transform_error(&t, raw[i], targets[i]) < 0.01);
    assert(touch_transform_error(&t, (touch_point_t){0, 0}, targets[4]) > 16);
    int32_t x, y;
    assert(touch_transform_apply(&t, 260, 600, 800, 480, &x, &y) && x == 400 && y == 240);
    assert(touch_transform_apply(&t, 0, 0, 800, 480, &x, &y) && x == 799 && y == 0);
    assert(touch_transform_apply(&t, 2000, 2000, 800, 480, &x, &y) && x == 0 && y == 479);
    assert(!touch_transform_apply(&t, 0, 0, 0, 480, &x, &y));
    // Nonuniform scale and skew, with light sample noise, remains accurate at center.
    for (int i = 0; i < 5; ++i)
        raw[i] = (touch_point_t){2 * targets[i].x + 0.1f * targets[i].y + 30,
                                 0.2f * targets[i].x + 1.5f * targets[i].y + 15};
    raw[0].x += 1;
    assert(touch_transform_fit(raw, targets, 4, &t));
    assert(touch_transform_error(&t, raw[4], targets[4]) < 1);
    for (int i = 0; i < 4; ++i)
        raw[i] = (touch_point_t){i * 100, i * 100};
    assert(!touch_transform_fit(raw, targets, 4, &t));
    for (int i = 0; i < 4; ++i)
        raw[i] = (touch_point_t){10, 10};
    assert(!touch_transform_fit(raw, targets, 4, &t));
    assert(!touch_transform_fit(raw, targets, 2, &t));
    raw[0].x = NAN;
    assert(!touch_transform_fit(raw, targets, 4, &t));
    t = (touch_transform_t){.a = NAN, .e = 1};
    assert(!touch_transform_apply(&t, 0, 0, 800, 480, &x, &y));
    t = (touch_transform_t){.a = 1, .b = 1, .d = 1, .e = 1};
    assert(!touch_transform_valid(&t));
}
static void capture_checks(void) {
    touch_capture_t c;
    touch_point_t p;
    touch_raw_sample_t up = {.valid = true, .fresh = true};
    touch_raw_sample_t down = {.x = 100, .y = 200, .contacts = 1, .valid = true, .fresh = true};
    touch_capture_reset(&c);
    assert(!touch_capture_update(&c, down, 0, &p)); // boot with held finger
    assert(!touch_capture_update(&c, down, 300, &p));
    assert(!touch_capture_update(&c, up, 310, &p));
    assert(!touch_capture_update(&c, down, 400, &p));
    assert(!touch_capture_update(&c, up, 500, &p)); // too short
    assert(!touch_capture_update(&c, down, 600, &p));
    down.fresh = false; // controller need not re-report a stationary finger
    assert(!touch_capture_update(&c, down, 900, &p));
    assert(touch_capture_update(&c, up, 910, &p));
    assert(p.x == 100 && p.y == 200);
    assert(!touch_capture_update(&c, up, 920, &p)); // once per press
    down.fresh = true;
    assert(!touch_capture_update(&c, down, 1000, &p));
    assert(!touch_capture_update(&c, down, 1300, &p));
    down.x = 200;
    assert(!touch_capture_update(&c, down, 1310, &p)); // moved after capture
    assert(!touch_capture_update(&c, up, 1320, &p));
    assert(!touch_capture_update(&c, down, 1400, &p));
    assert(!touch_capture_update(&c, down, 1700, &p));
    down.contacts = 2;
    assert(!touch_capture_update(&c, down, 1710, &p)); // multi-touch cancels
    assert(!touch_capture_update(&c, up, 1720, &p));
    down.contacts = 1;
    assert(!touch_capture_update(&c, down, 1800, &p));
    down.valid = false;
    assert(!touch_capture_update(&c, down, 2100, &p)); // bus fault cancels
    assert(!touch_capture_update(&c, up, 2110, &p));
    down.valid = true;
    assert(!touch_capture_update(&c, down, UINT32_MAX - 100, &p));
    assert(!touch_capture_update(&c, down, 200, &p)); // timestamp rollover
    assert(touch_capture_update(&c, up, 210, &p));
}
int main(void) {
    math_checks();
    capture_checks();
    puts("Touch math / capture: axes, scale, skew, error, clamp, degeneracy, hold/release, drag, multitouch, "
         "fault, rollover PASS");
}
