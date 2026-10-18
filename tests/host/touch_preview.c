#include "board/board.h"
#include "board/touch_settings.h"
#include "lvgl.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdio.h>

static bool enabled = true;
static unsigned applied, reads;
static touch_transform_t transform;
static lv_obj_t *calibration_screen;
static uint16_t pixels[800 * 480], draw_buffer[800 * 40];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x)
            pixels[y * 800 + x] = ((uint16_t *)p)[(y - a->y1) * (a->x2 - a->x1 + 1) + x - a->x1];
    lv_display_flush_ready(d);
}
void vTaskDelay(TickType_t ticks) {
    lv_tick_inc(ticks);
    assert(lv_tick_get() < 20000); // stalled calibration must fail the test
}
void board_touch_input_enable(bool value) {
    assert(enabled != value);
    enabled = value;
}
bool board_touch_apply_calibration(const touch_transform_t *value) {
    assert(touch_transform_valid(value));
    transform = *value;
    ++applied;
    return true;
}
touch_raw_sample_t board_touch_read_raw(void) {
    assert(!enabled);
    ++reads;
    if (!calibration_screen) {
        calibration_screen = lv_screen_active();
        lv_obj_null_on_delete(&calibration_screen);
    }
    const touch_point_t targets[] = {{64, 64}, {735, 64}, {735, 415}, {64, 415}, {400, 240}};
    uint32_t now = lv_tick_get();
    if (now == 150) {
        lv_refr_now(NULL);
        FILE *out = fopen("artifacts/touch-calibration.ppm", "wb");
        assert(out);
        fprintf(out, "P6\n800 480\n255\n");
        for (int i = 0; i < 800 * 480; ++i) {
            uint16_t v = pixels[i];
            uint8_t rgb[] = {((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31};
            fwrite(rgb, 1, 3, out);
        }
        fclose(out);
    }
    // Inject an I2C fault / held finger at boot. A release is required to arm.
    if (now < 50)
        return (touch_raw_sample_t){.contacts = 1, .valid = false};
    if (now < 100)
        return (touch_raw_sample_t){.contacts = 1, .valid = true, .fresh = true};
    if (now < 200)
        return (touch_raw_sample_t){.valid = true, .fresh = true};
    unsigned slot = (now - 200) / 450, phase = (now - 200) % 450;
    assert(slot < 10);
    if (phase >= 350)
        return (touch_raw_sample_t){.valid = true, .fresh = true};
    touch_point_t point = targets[slot % 5];
    if (slot == 4) // first center check is deliberately wrong; must retry all five
        point.x += 100;
    return (touch_raw_sample_t){
        .x = point.y + 20, .y = 1000 - point.x, .contacts = 1, .valid = true, .fresh = phase == 0};
}
int main(void) {
    lv_init();
    lv_display_t *display = lv_display_create(800, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_obj_t *previous = lv_screen_active();
    board_touch_setup();
    assert(enabled && applied == 1 && lv_screen_active() == previous);
    int32_t x, y;
    assert(touch_transform_apply(&transform, 260, 600, 800, 480, &x, &y) && x == 400 && y == 240);
#ifdef BOARD_TOUCH_CALIBRATE_ON_BOOT
    assert(reads > 400 && lv_tick_get() > 5500); // both attempts actually ran
    assert(calibration_screen == NULL);          // deletion event cleared the screen pointer
    puts("Calibration LVGL screen, rejected center/retry, apply, cleanup, input handoff PASS");
#else
    assert(reads == 0 && lv_tick_get() == 0);
    puts("Fixed mode: applies coefficients without calibration or raw touch polling PASS");
#endif
    lv_deinit();
}
