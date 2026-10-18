#include "board/board.h"
#include "board/board_config.h"
#include "board/touch_settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(BOARD_TOUCH_CALIBRATE_ON_BOOT) == defined(BOARD_TOUCH_USE_FIXED_CALIBRATION)
#error "Select exactly one touch mode in include/board/touch_settings.h"
#endif
#if defined(BOARD_TOUCH_USE_FIXED_CALIBRATION) && !defined(BOARD_TOUCH_CALIBRATION_COEFFICIENTS)
#error "Paste the measured BOARD_TOUCH_CALIBRATION_COEFFICIENTS from the serial monitor"
#endif

static const char *TAG = "touch_cal";

#ifdef BOARD_TOUCH_CALIBRATE_ON_BOOT
static lv_obj_t *text(lv_obj_t *parent, const char *value, int y, const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_width(label, BOARD_LCD_WIDTH - 160);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static void ui_step(void) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1);
}

static void calibrate(void) {
    static const touch_point_t targets[5] = {{64, 64},
                                             {BOARD_LCD_WIDTH - 65, 64},
                                             {BOARD_LCD_WIDTH - 65, BOARD_LCD_HEIGHT - 65},
                                             {64, BOARD_LCD_HEIGHT - 65},
                                             {BOARD_LCD_WIDTH / 2, BOARD_LCD_HEIGHT / 2}};
    touch_point_t samples[5];
    touch_transform_t result;
    touch_capture_t capture;
    touch_capture_reset(&capture);
    board_touch_input_enable(false);
    lv_obj_t *previous = lv_screen_active();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf3f7fa), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x193047), 0);
    text(screen, "Touch calibration", 112, &lv_font_montserrat_24);
    text(screen, "Hold the target with one finger, then release.", 154, &lv_font_montserrat_18);
    lv_obj_t *progress = text(screen, "", 300, &lv_font_montserrat_18);
    lv_obj_t *status = text(screen, "Release your finger to begin", 332, &lv_font_montserrat_18);
    text(screen, "Results will appear in the USB serial monitor.", 364, &lv_font_montserrat_18);
    lv_obj_t *target = lv_obj_create(screen);
    lv_obj_set_size(target, 48, 48);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(target, 3, 0);
    lv_obj_set_style_border_color(target, lv_color_hex(0x0e9482), 0);
    lv_obj_set_style_bg_color(target, lv_color_white(), 0);
    lv_obj_set_scrollable(target, false);
    lv_obj_t *cross = lv_label_create(target);
    lv_label_set_text(cross, "+");
    lv_obj_set_style_text_font(cross, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(cross, lv_color_hex(0x0e9482), 0);
    lv_obj_center(cross);
    lv_screen_load(screen);
    ESP_LOGI(TAG,
             "Calibration mode: four corners + independent center validation. Hold >=250 ms and release.");
    size_t index = 0;
    uint32_t last_error_log = lv_tick_get() - 5000;
    bool retry = false;
    while (1) {
        lv_obj_set_pos(target, targets[index].x - 24, targets[index].y - 24);
        lv_label_set_text_fmt(progress, "%sPoint %u / 5%s", retry ? "Please retry - " : "",
                              (unsigned)index + 1, index == 4 ? " - center check" : "");
        // Normal LVGL input is disabled; only this task consumes GT911 reports.
        touch_raw_sample_t raw = board_touch_read_raw();
        touch_point_t point;
        bool accepted = touch_capture_update(&capture, raw, lv_tick_get(), &point);
        if (!raw.valid) {
            lv_label_set_text(status, "Touch read failed - release and try again");
            if (lv_tick_elaps(last_error_log) >= 5000) {
                ESP_LOGW(TAG, "GT911 read failed; check touch I2C connection. Sample discarded.");
                last_error_log = lv_tick_get();
            }
        } else if (raw.contacts > 1) {
            lv_label_set_text(status, "Use one finger - release all fingers first");
        } else if (capture.phase == TOUCH_WAIT_RELEASE) {
            lv_label_set_text(status, "Release your finger to begin");
        } else if (capture.phase == TOUCH_READY) {
            lv_label_set_text(status, "Captured - lift your finger");
        } else if (capture.phase == TOUCH_HOLD) {
            lv_label_set_text_fmt(status, "Hold still... raw %u, %u", raw.x, raw.y);
        } else {
            lv_label_set_text(status, "Touch the center of the highlighted target");
        }
        lv_obj_set_style_bg_color(target, lv_color_hex(capture.phase == TOUCH_READY ? 0xb5eadf : 0xffffff),
                                  0);
        if (accepted) {
            samples[index] = point;
            ESP_LOGI(TAG, "Point %u: raw=(%.2f, %.2f) screen=(%.0f, %.0f)", (unsigned)index + 1, point.x,
                     point.y, targets[index].x, targets[index].y);
            if (++index == 5) {
                bool fit = touch_transform_fit(samples, targets, 4, &result);
                float worst = 0, center = INFINITY;
                if (fit) {
                    for (size_t i = 0; i < 4; ++i)
                        worst = fmaxf(worst, touch_transform_error(&result, samples[i], targets[i]));
                    center = touch_transform_error(&result, samples[4], targets[4]);
                }
                if (fit && worst <= 12 && center <= 16 && board_touch_apply_calibration(&result)) {
                    ESP_LOGI(TAG, "PASS: corner max error %.2f px; center error %.2f px", worst, center);
                    break;
                }
                ESP_LOGW(TAG, "Calibration rejected (fit=%d, corner=%.2f px, center=%.2f px). Retrying.", fit,
                         worst, center);
                index = 0;
                retry = true;
                touch_capture_reset(&capture);
            }
        }
        ui_step();
    }
    ESP_LOGI(TAG,
             "Applied for this session. Replace the mode/coefficients in include/board/touch_settings.h:");
    printf("\n// #define BOARD_TOUCH_CALIBRATE_ON_BOOT\n"
           "#define BOARD_TOUCH_USE_FIXED_CALIBRATION\n"
           "#define BOARD_TOUCH_CALIBRATION_COEFFICIENTS \\\n"
           "    { .a = %.9ff, .b = %.9ff, .c = %.9ff, \\\n"
           "      .d = %.9ff, .e = %.9ff, .f = %.9ff }\n\n",
           result.a, result.b, result.c, result.d, result.e, result.f);
    fflush(stdout);
    lv_obj_set_hidden(target, true);
    lv_label_set_text(progress, "Calibration complete");
    lv_label_set_text(status, "Copy the serial output before the next build");
    uint32_t done = lv_tick_get();
    while (lv_tick_elaps(done) < 1200)
        ui_step();
    lv_screen_load(previous);
    lv_obj_delete(screen);
    board_touch_input_enable(true);
}
#endif

void board_touch_setup(void) {
#ifdef BOARD_TOUCH_CALIBRATE_ON_BOOT
    calibrate();
#else
    static const touch_transform_t fixed = BOARD_TOUCH_CALIBRATION_COEFFICIENTS;
    if (!board_touch_apply_calibration(&fixed)) {
        ESP_LOGE(TAG,
                 "Invalid fixed touch coefficients. Correct include/board/touch_settings.h and rebuild.");
        abort();
    }
    ESP_LOGI(TAG, "Fixed calibration: a=%.9f b=%.9f c=%.9f d=%.9f e=%.9f f=%.9f", fixed.a, fixed.b, fixed.c,
             fixed.d, fixed.e, fixed.f);
#endif
}
