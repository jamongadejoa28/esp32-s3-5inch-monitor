#include "board/board.h"
#include "board/clock.h"
#include "ui/dashboard.h"
#include "services/service.h"
#include "services/storage.h"
#include "services/settings_sync.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

_Static_assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 6 && LVGL_VERSION_PATCH == 0,
               "This firmware requires LVGL 9.6.0");

static void log_memory(const char *reason) {
    // ESP-IDF reports bytes here (upstream FreeRTOS ports may report words).
    uint32_t ui_free = uxTaskGetStackHighWaterMark(NULL);
    uint32_t bus_free = service_stack_free_min();
    ESP_LOGI("memory",
             "%s: stack_min_free main=%lu B modbus=%lu B; internal_free=%u B largest=%u B; PSRAM_free=%u B",
             reason, (unsigned long)ui_free, (unsigned long)bus_free,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (ui_free < 2048 || bus_free < 768)
        ESP_LOGW("memory", "Low stack reserve: inspect the active screen/task before reducing stack sizes.");
}

void app_main(void) {
    storage_init();
    board_init();
    board_touch_setup();
    service_init();
    service_set_unit(storage_config()->unit);
    settings_sync_init();
    dashboard_init();
    log_memory("startup");
    uint32_t logged_at = lv_tick_get();
    lv_obj_t *screen = lv_screen_active();
    while (1) {
        board_clock_poll();
        dashboard_tick();
        uint32_t ms = lv_timer_handler();
        bool screen_changed = screen != lv_screen_active();
        if (screen_changed || lv_tick_elaps(logged_at) >= 30000) {
            log_memory(screen_changed ? "screen change" : "periodic");
            screen = lv_screen_active();
            logged_at = lv_tick_get();
        }
        if (ms > 10)
            ms = 10;
        if (ms < 1)
            ms = 1;
        vTaskDelay(pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1);
    }
}
