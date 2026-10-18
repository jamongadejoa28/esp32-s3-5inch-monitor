// Pin/timing reference: Waveshare ESP32-S3-Touch-LCD-5 official LVGL9 demo.
// IDF 6.1 uses the new I2C master API (not the removed legacy driver).
#include "board/board.h"
#include "board/board_config.h"
#include "board/clock.h"
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <sys/time.h>
#include <string.h>
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t exp_mode, exp_out, touch, rtc;
static esp_lcd_panel_handle_t panel;
static uint8_t outputs = 0x1e;
static lv_indev_t *touch_input;
static touch_raw_sample_t raw_touch;
static touch_transform_t touch_transform = {.a = 1, .e = 1};
static esp_err_t add_device(int address, i2c_master_dev_handle_t *d) {
    i2c_device_config_t c = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = address, .scl_speed_hz = BOARD_I2C_HZ};
    return i2c_master_bus_add_device(bus, &c, d);
}
void board_display_on(bool on) {
    uint8_t v = on ? (outputs | BOARD_BACKLIGHT_MASK) : (outputs & ~BOARD_BACKLIGHT_MASK);
    if (i2c_master_transmit(exp_out, &v, 1, 50) == ESP_OK)
        outputs = v;
}
static bool touch_read(uint16_t reg, void *out, size_t n) {
    uint8_t a[] = {reg >> 8, reg};
    return i2c_master_transmit_receive(touch, a, 2, out, n, 20) == ESP_OK;
}
touch_raw_sample_t board_touch_read_raw(void) {
    uint8_t status = 0, buf[8];
    raw_touch.fresh = false;
    if (!touch_read(0x814e, &status, 1)) {
        raw_touch.valid = false;
    } else if (status & 0x80) {
        raw_touch.fresh = true;
        raw_touch.contacts = status & 15;
        raw_touch.valid = raw_touch.contacts <= 5;
        if (raw_touch.contacts == 1) {
            raw_touch.valid = touch_read(0x8150, buf, 8);
            if (raw_touch.valid) {
                raw_touch.x = buf[0] | buf[1] << 8;
                raw_touch.y = buf[2] | buf[3] << 8;
            }
        }
        uint8_t clear[] = {0x81, 0x4e, 0};
        if (i2c_master_transmit(touch, clear, 3, 20) != ESP_OK)
            raw_touch.valid = false;
    } else if (raw_touch.contacts == 0) {
        // GT911 may have no pending report while idle at startup.
        raw_touch.valid = true;
    }
    return raw_touch;
}
bool board_touch_apply_calibration(const touch_transform_t *transform) {
    if (!touch_transform_valid(transform))
        return false;
    touch_transform = *transform;
    return true;
}
void board_touch_input_enable(bool enabled) {
    lv_indev_reset(touch_input, NULL);
    lv_indev_enable(touch_input, enabled);
}
static void input(lv_indev_t *dev, lv_indev_data_t *data) {
    (void)dev;
    static lv_point_t last;
    touch_raw_sample_t s = board_touch_read_raw();
    int32_t x, y;
    bool down = s.valid && s.contacts == 1 &&
                touch_transform_apply(&touch_transform, s.x, s.y, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, &x, &y);
    if (down)
        last = (lv_point_t){x, y};
    data->point = last;
    data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void flush(lv_display_t *disp, const lv_area_t *a, uint8_t *p) {
    esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, p);
    lv_display_flush_ready(disp);
}
static uint32_t ticks(void) {
    return esp_timer_get_time() / 1000;
}
void board_init(void) {
    i2c_master_bus_config_t b = {.i2c_port = BOARD_I2C_PORT,
                                 .sda_io_num = BOARD_I2C_SDA_GPIO,
                                 .scl_io_num = BOARD_I2C_SCL_GPIO,
                                 .clk_source = I2C_CLK_SRC_DEFAULT,
                                 .glitch_ignore_cnt = 7,
                                 .flags.enable_internal_pullup = true};
    ESP_ERROR_CHECK(i2c_new_master_bus(&b, &bus));
    ESP_ERROR_CHECK(add_device(BOARD_EXPANDER_MODE_ADDR, &exp_mode));
    ESP_ERROR_CHECK(add_device(BOARD_EXPANDER_OUT_ADDR, &exp_out));
    ESP_ERROR_CHECK(add_device(BOARD_TOUCH_ADDR, &touch));
    ESP_ERROR_CHECK(add_device(BOARD_RTC_ADDR, &rtc));
    uint8_t v = 1;
    ESP_ERROR_CHECK(i2c_master_transmit(exp_mode, &v, 1, 100));
    gpio_set_direction(BOARD_TOUCH_INT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_TOUCH_INT_GPIO, 0);
    v = 0x2c;
    ESP_ERROR_CHECK(i2c_master_transmit(exp_out, &v, 1, 100));
    vTaskDelay(pdMS_TO_TICKS(100));
    v = 0x2e;
    ESP_ERROR_CHECK(i2c_master_transmit(exp_out, &v, 1, 100));
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_direction(BOARD_TOUCH_INT_GPIO, GPIO_MODE_INPUT);
    ESP_ERROR_CHECK(i2c_master_transmit(exp_out, &outputs, 1, 100));
    board_clock_init(rtc);
    esp_lcd_rgb_panel_config_t pc = {.clk_src = LCD_CLK_SRC_DEFAULT,
                                     .timings = {.pclk_hz = BOARD_LCD_PCLK_HZ,
                                                 .h_res = BOARD_LCD_WIDTH,
                                                 .v_res = BOARD_LCD_HEIGHT,
                                                 .hsync_pulse_width = BOARD_LCD_HSYNC_PULSE,
                                                 .hsync_back_porch = BOARD_LCD_HSYNC_BACK_PORCH,
                                                 .hsync_front_porch = BOARD_LCD_HSYNC_FRONT_PORCH,
                                                 .vsync_pulse_width = BOARD_LCD_VSYNC_PULSE,
                                                 .vsync_back_porch = BOARD_LCD_VSYNC_BACK_PORCH,
                                                 .vsync_front_porch = BOARD_LCD_VSYNC_FRONT_PORCH,
                                                 .flags.pclk_active_neg = 1},
                                     .data_width = 16,
                                     .in_color_format = LCD_COLOR_FMT_RGB565,
                                     .out_color_format = LCD_COLOR_FMT_RGB565,
                                     .num_fbs = 1,
                                     .bounce_buffer_size_px = BOARD_LCD_WIDTH * BOARD_LCD_BOUNCE_LINES,
                                     .hsync_gpio_num = BOARD_LCD_HSYNC_GPIO,
                                     .vsync_gpio_num = BOARD_LCD_VSYNC_GPIO,
                                     .de_gpio_num = BOARD_LCD_DE_GPIO,
                                     .pclk_gpio_num = BOARD_LCD_PCLK_GPIO,
                                     .disp_gpio_num = BOARD_LCD_DISP_GPIO,
                                     .data_gpio_nums = BOARD_LCD_DATA_GPIOS,
                                     .flags.fb_in_psram = true};
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&pc, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    lv_init();
    lv_tick_set_cb(ticks);
    lv_display_t *disp = lv_display_create(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    void *pixels = heap_caps_malloc(BOARD_LCD_WIDTH * BOARD_LVGL_DRAW_LINES * BOARD_LCD_BYTES_PER_PIXEL,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(pixels);
    lv_display_set_buffers(disp, pixels, NULL,
                           BOARD_LCD_WIDTH * BOARD_LVGL_DRAW_LINES * BOARD_LCD_BYTES_PER_PIXEL,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    touch_input = lv_indev_create();
    lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_input, input);
}
