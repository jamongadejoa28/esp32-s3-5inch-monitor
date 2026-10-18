#include "lvgl.h"
#include "ui/dashboard.h"
#include "ui/alarms.h"
#include "services/service.h"
#include "services/storage.h"
#include "services/settings_sync.h"
#include "board/board.h"
#include "screens.h"
#include "actions.h"
#include "fonts.h"
#include "glyphs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
static mex_snapshot_t fixture;
static command_state_t status;
static uint64_t now_ms;
static uint16_t pixels[800 * 480];
static lv_indev_data_t pointer;
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    data->point = pointer.point;
    data->state = pointer.state;
}
int64_t esp_timer_get_time(void) {
    return now_ms * 1000;
}
void service_snapshot(mex_snapshot_t *s) {
    *s = fixture;
}
command_state_t service_result(void) {
    return status;
}
void service_init(void) {}
uint8_t service_unit(void) {
    return 1;
}
void service_set_unit(uint8_t u) {
    (void)u;
}
bool service_send(uint8_t fc, uint16_t a, int32_t v, bool stop) {
    mex_command_t c = {fc, a, v, stop, 0};
    if (!mex_validate(&c, fixture.online, mex_running(&fixture))) {
        status = CMD_REJECTED;
        return false;
    }
    if (fc == 6)
        fixture.regs[a - 40000] = (uint16_t)v;
    else {
        fixture.coils &= ~(1 << (a - 1));
        fixture.coils |= v << (a - 1);
    }
    status = CMD_APPLIED;
    return true;
}
void board_display_on(bool on) {
    (void)on;
}
bool board_clock_valid(void) {
    return true;
}
bool board_clock_set(struct tm *t) {
    return t->tm_mday <= 31;
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            pixels[y * 800 + x] = ((uint16_t *)p)[(y - a->y1) * (a->x2 - a->x1 + 1) + x - a->x1];
    lv_display_flush_ready(d);
}
static void tick(int n) {
    for (int i = 0; i < n; i++) {
        now_ms += 20;
        fixture.received_ms = now_ms;
        lv_tick_inc(20);
        if (now_ms % 1000 == 0)
            fixture.sequence++;
        dashboard_tick();
        lv_timer_handler();
    }
}
static void capture(const char *name) {
    lv_refr_now(NULL);
    char path[128];
    snprintf(path, sizeof(path), "artifacts/%s.ppm", name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n800 480\n255\n");
    for (int i = 0; i < 800 * 480; i++) {
        uint16_t v = pixels[i];
        uint8_t rgb[] = {((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void shot(const char *name) {
    tick(20);
    capture(name);
}
static lv_obj_t *find_text(lv_obj_t *p, const char *s) {
    if (lv_obj_check_type(p, &lv_label_class) && !strcmp(lv_label_get_text(p), s))
        return p;
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++) {
        lv_obj_t *r = find_text(lv_obj_get_child(p, i), s);
        if (r)
            return r;
    }
    return NULL;
}
static void click_text(lv_obj_t *p, const char *s) {
    lv_obj_t *l = find_text(p, s);
    assert(l);
    lv_obj_send_event(lv_obj_get_parent(l), LV_EVENT_CLICKED, NULL);
    tick(2);
}
static lv_obj_t *find_type(lv_obj_t *parent, const lv_obj_class_t *type) {
    if (lv_obj_check_type(parent, type))
        return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
        lv_obj_t *found = find_type(lv_obj_get_child(parent, i), type);
        if (found)
            return found;
    }
    return NULL;
}
// Use LVGL pointer hit testing at the visible label, not a synthetic CLICKED event.
static void tap(lv_obj_t *button) {
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area;
    lv_obj_get_coords(lv_obj_get_child(button, 0), &area);
    pointer.point.x = (area.x1 + area.x2) / 2;
    pointer.point.y = (area.y1 + area.y2) / 2;
    pointer.state = LV_INDEV_STATE_PRESSED;
    tick(4);
    pointer.state = LV_INDEV_STATE_RELEASED;
    tick(4);
}
static void assert_selection(lv_obj_t *const *tabs, size_t count, size_t selected) {
    lv_obj_update_layout(lv_screen_active());
    for (size_t i = 0; i < count; i++) {
        assert(lv_obj_has_state(tabs[i], LV_STATE_CHECKED) == (i == selected));
        lv_area_t button, label;
        lv_obj_get_coords(tabs[i], &button);
        lv_obj_get_coords(lv_obj_get_child(tabs[i], 0), &label);
        assert(abs((button.y1 + button.y2) - (label.y1 + label.y2)) <= 2);
        assert(abs((button.x1 + button.x2) - (label.x1 + label.x2)) <= 2);
    }
}
static void check_panel_tabs(void) {
    lv_obj_t *history[] = {objects.history_tab_0, objects.history_tab_1, objects.history_tab_2,
                           objects.history_tab_3};
    lv_obj_t *equipment[] = {objects.equipment_tab_0, objects.equipment_tab_1, objects.equipment_tab_2,
                             objects.equipment_tab_3};
    lv_obj_t *settings[] = {objects.settings_tab_0, objects.settings_tab_1, objects.settings_tab_2,
                            objects.settings_tab_3, objects.settings_tab_4, objects.settings_tab_5};
    const char *first_setting[] = {"설정온도", "냉방 표시모드",      "운전모드",
                                   "화면밝기", "일요일 운전 (HHMM)", "년"};
    action_nav_history(NULL);
    tick(20);
    for (int i = 0; i < 4; i++) {
        tap(history[i]);
        assert_selection(history, 4, i);
        lv_obj_t *chart = find_type(objects.history_body, &lv_chart_class);
        assert(chart);
        if (i == 0)
            assert(lv_chart_get_point_count(chart) > 2);
        tap(objects.history_tab_4);
        assert(lv_obj_has_state(objects.history_tab_4, LV_STATE_CHECKED));
        lv_obj_t *table = find_type(objects.history_body, &lv_table_class);
        assert(table && lv_table_get_column_count(table) == 5);
        assert(!find_type(objects.history_body, &lv_chart_class));
        tap(objects.history_tab_4);
        assert(!lv_obj_has_state(objects.history_tab_4, LV_STATE_CHECKED));
        assert(find_type(objects.history_body, &lv_chart_class));
    }
    action_nav_equipment(NULL);
    tick(20);
    for (int i = 0; i < 4; i++) {
        tap(equipment[i]);
        assert_selection(equipment, 4, i);
        if (i == 0)
            assert(find_text(objects.equipment_body, "모니터 적산 초기화"));
        if (i == 1)
            assert(find_text(objects.equipment_body, "현재 경보 없음"));
        if (i == 2)
            assert(find_text(objects.equipment_body, "예약은 모니터에서 실행 · 같은 시각은 비활성"));
        if (i == 3) {
            lv_obj_t *manual = find_text(objects.equipment_body, "송풍 · ON");
            assert(manual && lv_obj_has_state(lv_obj_get_parent(manual), LV_STATE_DISABLED));
        }
    }
    action_nav_settings(NULL);
    tick(20);
    for (int i = 0; i < 6; i++) {
        tap(settings[i]);
        assert_selection(settings, 6, i);
        assert(find_text(objects.settings_body, first_setting[i]));
        assert(lv_obj_get_scroll_y(objects.settings_body) == 0);
        lv_obj_scroll_to_y(objects.settings_body, 200, LV_ANIM_OFF);
        tick(2);
    }
    // Re-entering a page preserves its selected category and button instances.
    action_nav_main(NULL);
    tick(20);
    action_nav_settings(NULL);
    tick(20);
    assert_selection(settings, 6, 5);
    assert(find_text(objects.settings_body, "년"));
    tap(settings[0]);
    // Language changes do not break event bindings or selected state.
    click_text(objects.settings_content, "화면");
    storage_config()->language = 1;
    action_nav_settings(NULL);
    tick(20);
    tap(settings[5]);
    assert(find_text(objects.settings_body, "Year"));
    storage_config()->language = 0;
    tap(settings[0]);
    action_nav_history(NULL);
    tick(20);
    tap(history[0]);
    action_nav_equipment(NULL);
    tick(20);
    tap(equipment[0]);
    action_nav_main(NULL);
    tick(20);
    puts("15 EEZ tabs: pointer hit testing, content, radio selection, history table, scrolling and re-entry "
         "passed");
}
static void check_clock_navigation(void) {
    const struct {
        void (*action)(lv_event_t *);
        lv_obj_t *screen;
    } pages[] = {{action_nav_history, objects.history},
                 {action_nav_equipment, objects.equipment},
                 {action_nav_settings, objects.settings},
                 {action_nav_main, objects.main}};
    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i) {
        pages[i].action(NULL);
        // No timer advance: the requested screen must already be fully visible.
        assert(lv_screen_active() == pages[i].screen);
        assert(lv_obj_get_style_opa(pages[i].screen, 0) == LV_OPA_COVER);
    }
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char expected[40];
    strftime(expected, sizeof(expected), "%Y-%m-%d %H:%M KST", &local);
    lv_obj_t *clocks[] = {objects.main_clock, objects.history_clock, objects.equipment_clock,
                          objects.settings_clock};
    for (size_t i = 0; i < 4; ++i) {
        assert(!strcmp(lv_label_get_text(clocks[i]), expected));
        lv_point_t size;
        lv_text_get_size(&size, expected, &ui_font_body, 0, 0, LV_COORD_MAX, 0);
        assert(size.x <= lv_obj_get_width(clocks[i]) && size.y <= lv_obj_get_height(clocks[i]));
    }
    puts("Year + KST clock fits all four headers; immediate navigation without fade PASS");
}
static void alarm_frame(unsigned mask) {
    fixture.online = true;
    fixture.regs[3] = mask;
    fixture.sequence++;
    tick(1);
}
static void check_alarm_controls(void) {
    action_nav_equipment(NULL);
    click_text(objects.equipment_content, "경보");
    alarm_frame(32);
    lv_obj_t *next = lv_obj_get_parent(find_text(objects.equipment_body, "다음"));
    assert(lv_obj_has_state(next, LV_STATE_DISABLED));
    click_text(objects.equipment_body, "경보해제");
    click_text(lv_obj_get_child(lv_layer_top(), -1), "적용");
    assert(find_text(objects.equipment_body, "알림 해제 · 재조회 대기"));
    alarm_frame(32);
    assert(find_text(objects.equipment_body, "발생 · 화재"));
    alarm_frame(0);
    assert(find_text(objects.equipment_body, "현재 경보 없음"));
    lv_obj_t *table = find_type(objects.equipment_body, &lv_table_class);
    assert(table && !strcmp(lv_table_get_cell_value(table, 1, 2), "해제"));
    shot("alarm-cleared");
    for (unsigned i = 1; i < 50; ++i) {
        alarm_frame(32);
        alarm_frame(0);
    }
    table = find_type(objects.equipment_body, &lv_table_class);
    assert(table && lv_table_get_row_count(table) == 51);
    next = lv_obj_get_parent(find_text(objects.equipment_body, "다음"));
    assert(lv_obj_has_state(next, LV_STATE_DISABLED)); // a full first page alone isn't a next page
    alarm_frame(32);
    next = lv_obj_get_parent(find_text(objects.equipment_body, "다음"));
    assert(!lv_obj_has_state(next, LV_STATE_DISABLED));
    click_text(objects.equipment_body, "다음");
    assert(find_text(objects.equipment_body, "2 / 2  (51)"));
    table = find_type(objects.equipment_body, &lv_table_class);
    assert(table && lv_table_get_row_count(table) == 2);
    click_text(objects.equipment_body, "이력 삭제");
    click_text(lv_obj_get_child(lv_layer_top(), -1), "취소");
    assert(find_text(objects.equipment_body, "2 / 2  (51)"));
    click_text(objects.equipment_body, "이력 삭제");
    click_text(lv_obj_get_child(lv_layer_top(), -1), "삭제");
    assert(find_text(objects.equipment_body, "경보 이력 없음"));
    assert(find_text(objects.equipment_body, "발생 · 화재"));
    assert(find_text(objects.equipment_body, "1 / 1  (0)"));
    shot("alarm-history-deleted");
    alarm_frame(0);
    puts("Alarm GUI: acknowledgment, next-frame re-detection, cleared row, actual 50/51 pagination and "
         "confirmed deletion PASS");
}
static void check_offline_settings(void) {
    fixture.online = false;
    tick(55);
    action_nav_settings(NULL);
    click_text(objects.settings_content, "기본");
    const char *labels[] = {"설정온도", "설정습도", "냉방편차", "난방편차",
                            "가습편차", "제습편차", "운전지연", "정지지연"};
    const char *values[] = {"26.7", "55.5", "1.5", "2.5", "3.5", "4.5", "12", "18"};
    const int raw[] = {267, 555, 15, 25, 35, 45, 12, 18};
    int previous_temp = fixture.regs[12];
    for (unsigned i = 0; i < 8; ++i) {
        lv_obj_t *label = find_text(objects.settings_body, labels[i]);
        assert(label);
        lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, NULL);
        lv_obj_t *input = find_type(lv_layer_top(), &lv_textarea_class);
        assert(input); // disconnected fields must actually open an editor
        lv_textarea_set_text(input, values[i]);
        lv_obj_t *card = lv_obj_get_parent(input);
        click_text(card, "저장");
        assert(!find_type(lv_layer_top(), &lv_textarea_class));
    }
    assert(settings_sync_pending() == 8 && fixture.regs[12] == previous_temp);
    assert(find_text(objects.settings_body, "26.7 *"));
    shot("settings-offline-pending");
    settings_sync_init();
    assert(settings_sync_pending() == 8);
    fixture.online = true;
    tick(100);
    assert(settings_sync_pending() == 0);
    for (unsigned i = 0; i < 8; ++i)
        assert(fixture.regs[12 + i] == raw[i]);
    assert(fixture.regs[5] == 1); // settings replay never changes the run/stop register
    action_nav_settings(NULL);
    assert(find_text(objects.settings_body, "26.7"));
    shot("settings-synchronized");
    puts("Eight disconnected GUI editors, pending indicators, persistence and automatic readback "
         "synchronization PASS");
}
int main(void) {
    assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 6 && LVGL_VERSION_PATCH == 0);
    setenv("TZ", "KST-9", 1);
    tzset();
    storage_init();
    assert(ui_alarm_page_count(0) == 1 && ui_alarm_page_count(49) == 1);
    assert(ui_alarm_page_count(50) == 1 && ui_alarm_page_count(51) == 2);
    assert(ui_alarm_page_count(500) == 10);
    lv_init();
    lv_display_t *d = lv_display_create(800, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(d, flush);
    static uint16_t draw[800 * 40];
    lv_display_set_buffers(d, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t *input_device = lv_indev_create();
    lv_indev_set_type(input_device, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input_device, read_pointer);
    for (unsigned i = 0; i < sizeof(glyph_codes) / sizeof(*glyph_codes); i++) {
        lv_font_glyph_dsc_t g;
        bool ok = lv_font_get_glyph_dsc(&ui_font_body, &g, glyph_codes[i], 0);
        if (!ok || g.is_placeholder) {
            fprintf(stderr, "Missing glyph U+%04X\n", glyph_codes[i]);
            return 2;
        }
    }
    fixture.online = true;
    fixture.sequence = 1;
    fixture.regs[36] = 235;
    fixture.regs[37] = 463;
    fixture.regs[12] = 230;
    fixture.regs[13] = 450;
    fixture.regs[14] = 10;
    fixture.regs[15] = 10;
    fixture.regs[16] = 30;
    fixture.regs[17] = 30;
    fixture.regs[20] = 1;
    fixture.regs[5] = 1;
    fixture.regs[6] = 2;
    fixture.regs[7] = 0;
    fixture.regs[8] = 1;
    fixture.coils = 1 | 4 | 128;
    dashboard_init();
    for (int i = 0; i < 180; i++) {
        fixture.regs[36] = 235 + sin(i / 18.0) * 12;
        fixture.regs[37] = 463 + sin(i / 25.0) * 20;
        tick(50);
    }
    check_panel_tabs();
    check_clock_navigation();
    shot("dashboard-live");
    // Animation frame screenshots must change with active state.
    shot("animation-a");
    tick(5);
    shot("animation-b");
    for (int i = 0; i < 12; i++) {
        char name[64];
        snprintf(name, sizeof(name), "motion-%02d", i);
        tick(5);
        capture(name);
    }
    action_nav_history(NULL);
    shot("history-live");
    action_nav_equipment(NULL);
    shot("equipment-live");
    click_text(objects.equipment_content, "경보");
    fixture.regs[3] = 1 << 5;
    tick(55);
    shot("alarm-live");
    check_alarm_controls();
    action_nav_settings(NULL);
    shot("settings-live");
    // Open a real editor, reject out-of-range setting, then accept 25.0 C.
    lv_obj_send_event(lv_obj_get_child(objects.settings_body, 0), LV_EVENT_CLICKED, NULL);
    shot("setting-editor");
    lv_obj_t *modal = lv_obj_get_child(lv_layer_top(), -1);
    lv_obj_t *card = lv_obj_get_child(modal, 0), *input = NULL;
    for (unsigned i = 0; i < lv_obj_get_child_count(card); i++) {
        lv_obj_t *o = lv_obj_get_child(card, i);
        if (lv_obj_check_type(o, &lv_textarea_class))
            input = o;
    }
    assert(input);
    lv_textarea_set_text(input, "99");
    click_text(card, "저장");
    assert(fixture.regs[12] == 230);
    lv_textarea_set_text(input, "25.0");
    click_text(card, "저장");
    assert(fixture.regs[12] == 250);
    action_nav_main(NULL);
    storage_config()->mode[0] = 1;
    tick(60);
    shot("proportional-live");
    fixture.online = false;
    tick(60);
    shot("offline-live");
    assert(!service_send(6, 40005, 1, false));
    check_offline_settings();
    puts("LVGL runtime navigation, input validation, animation, proportional and offline checks passed");
    return 0;
}
