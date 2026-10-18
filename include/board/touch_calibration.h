#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float x, y;
} touch_point_t;
typedef struct {
    float a, b, c, d, e, f;
} touch_transform_t;
typedef struct {
    uint16_t x, y;
    uint8_t contacts;
    bool valid, fresh;
} touch_raw_sample_t;

// Four corner samples define the affine map; the fifth center sample validates it.
bool touch_transform_fit(const touch_point_t *raw, const touch_point_t *screen, size_t count,
                         touch_transform_t *out);
bool touch_transform_valid(const touch_transform_t *transform);
float touch_transform_error(const touch_transform_t *transform, touch_point_t raw, touch_point_t screen);
bool touch_transform_apply(const touch_transform_t *transform, uint16_t raw_x, uint16_t raw_y, int width,
                           int height, int32_t *x, int32_t *y);

typedef enum { TOUCH_WAIT_RELEASE, TOUCH_WAIT_PRESS, TOUCH_HOLD, TOUCH_READY } touch_capture_phase_t;
typedef struct {
    touch_capture_phase_t phase;
    uint32_t since_ms, samples;
    float anchor_x, anchor_y;
    double sum_x, sum_y;
} touch_capture_t;

void touch_capture_reset(touch_capture_t *capture);
// Returns one accepted point only after a stable hold (250 ms) followed by release.
bool touch_capture_update(touch_capture_t *capture, touch_raw_sample_t sample, uint32_t now_ms,
                          touch_point_t *point);

// Called after board_init(), before starting the dashboard / controller service.
void board_touch_setup(void);
