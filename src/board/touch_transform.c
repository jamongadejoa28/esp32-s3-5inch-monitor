#include "board/touch_calibration.h"
#include <math.h>
#include <string.h>

bool touch_transform_valid(const touch_transform_t *t) {
    return t && isfinite(t->a) && isfinite(t->b) && isfinite(t->c) && isfinite(t->d) && isfinite(t->e) &&
           isfinite(t->f) && isfinite(t->a * t->e - t->b * t->d) && fabsf(t->a * t->e - t->b * t->d) > 1e-8f;
}

bool touch_transform_fit(const touch_point_t *raw, const touch_point_t *screen, size_t count,
                         touch_transform_t *out) {
    if (!raw || !screen || !out || count < 3)
        return false;
    double mx = 0, my = 0, tx = 0, ty = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(raw[i].x) || !isfinite(raw[i].y) || !isfinite(screen[i].x) || !isfinite(screen[i].y))
            return false;
        mx += raw[i].x;
        my += raw[i].y;
        tx += screen[i].x;
        ty += screen[i].y;
    }
    mx /= count;
    my /= count;
    tx /= count;
    ty /= count;
    double xx = 0, xy = 0, yy = 0, u_x = 0, u_y = 0, v_x = 0, v_y = 0;
    for (size_t i = 0; i < count; ++i) {
        double x = raw[i].x - mx, y = raw[i].y - my;
        double u = screen[i].x - tx, v = screen[i].y - ty;
        xx += x * x;
        xy += x * y;
        yy += y * y;
        u_x += u * x;
        u_y += u * y;
        v_x += v * x;
        v_y += v * y;
    }
    double det = xx * yy - xy * xy;
    if (xx < 1 || yy < 1 || det <= 1e-6 * xx * yy)
        return false;
    touch_transform_t t = {.a = (u_x * yy - u_y * xy) / det,
                           .b = (u_y * xx - u_x * xy) / det,
                           .d = (v_x * yy - v_y * xy) / det,
                           .e = (v_y * xx - v_x * xy) / det};
    t.c = tx - t.a * mx - t.b * my;
    t.f = ty - t.d * mx - t.e * my;
    if (!touch_transform_valid(&t))
        return false;
    *out = t;
    return true;
}

float touch_transform_error(const touch_transform_t *t, touch_point_t raw, touch_point_t screen) {
    if (!touch_transform_valid(t))
        return INFINITY;
    return hypotf(t->a * raw.x + t->b * raw.y + t->c - screen.x,
                  t->d * raw.x + t->e * raw.y + t->f - screen.y);
}

bool touch_transform_apply(const touch_transform_t *t, uint16_t raw_x, uint16_t raw_y, int width, int height,
                           int32_t *x, int32_t *y) {
    if (!touch_transform_valid(t) || !x || !y || width <= 0 || height <= 0)
        return false;
    float px = t->a * raw_x + t->b * raw_y + t->c;
    float py = t->d * raw_x + t->e * raw_y + t->f;
    if (!isfinite(px) || !isfinite(py))
        return false;
    // Clamp after transformation, so swapped axes and offsets remain calibratable.
    *x = lroundf(fminf(fmaxf(px, 0), width - 1));
    *y = lroundf(fminf(fmaxf(py, 0), height - 1));
    return true;
}

void touch_capture_reset(touch_capture_t *c) {
    memset(c, 0, sizeof(*c));
    c->phase = TOUCH_WAIT_RELEASE;
}

static void start_hold(touch_capture_t *c, touch_raw_sample_t s, uint32_t now) {
    c->phase = TOUCH_HOLD;
    c->since_ms = now;
    c->anchor_x = s.x;
    c->anchor_y = s.y;
    c->sum_x = s.x;
    c->sum_y = s.y;
    c->samples = 1;
}

bool touch_capture_update(touch_capture_t *c, touch_raw_sample_t s, uint32_t now, touch_point_t *point) {
    if (!s.valid || s.contacts > 1) {
        touch_capture_reset(c);
        return false;
    }
    if (s.contacts == 0) {
        bool accepted = c->phase == TOUCH_READY;
        if (accepted)
            *point = (touch_point_t){c->sum_x / c->samples, c->sum_y / c->samples};
        c->phase = TOUCH_WAIT_PRESS;
        return accepted;
    }
    if (c->phase == TOUCH_WAIT_RELEASE)
        return false;
    if (c->phase == TOUCH_WAIT_PRESS) {
        if (s.fresh)
            start_hold(c, s, now);
        return false;
    }
    if (fabsf(s.x - c->anchor_x) > 12 || fabsf(s.y - c->anchor_y) > 12) {
        start_hold(c, s, now);
        return false;
    }
    if (c->phase == TOUCH_HOLD) {
        if (s.fresh) {
            c->sum_x += s.x;
            c->sum_y += s.y;
            ++c->samples;
        }
        if ((uint32_t)(now - c->since_ms) >= 250)
            c->phase = TOUCH_READY;
    }
    return false;
}
