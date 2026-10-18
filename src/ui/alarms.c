#include "ui/alarms.h"
#include "ui/widgets.h"
#include "services/storage.h"
#include "esp_timer.h"
#include "fonts.h"
#include <stdio.h>

// Single UI-task owner. This buffer is separate from transient chart workspaces.
static alarm_history_entry_t entries[ALARM_HISTORY_LIMIT];
static lv_obj_t *host;
static bool language;
static size_t page;
static lv_event_cb_t clear_action, acknowledge_action;
static const char *names_ko[] = {"송풍기", "냉방 1", "냉방 2", "난방", "가습", "화재", "누수", "실린더 오염",
                                 "급수",   "배수",   "냉방 3", "센서", "고온", "저온", "고습", "저습"};
static const char *names_en[] = {
    "Fan",         "Cooling 1", "Cooling 2",        "Heater",          "Humidifier",
    "Fire",        "Leak",      "Cylinder fouling", "Supply",          "Drain",
    "Cooling 3",   "Sensor",    "High temperature", "Low temperature", "High humidity",
    "Low humidity"};
static const char *tr(const char *ko, const char *en) {
    return language ? en : ko;
}
size_t ui_alarm_page_count(size_t count) {
    return count ? (count + ALARM_PAGE_SIZE - 1) / ALARM_PAGE_SIZE : 1;
}
void ui_alarms_first_page(void) {
    page = 0;
}
static void paging(lv_event_t *event) {
    intptr_t direction = (intptr_t)lv_event_get_user_data(event);
    size_t count = storage_alarm_history(entries, ALARM_HISTORY_LIMIT);
    size_t pages = ui_alarm_page_count(count);
    if (direction < 0 && page)
        --page;
    else if (direction > 0 && page + 1 < pages)
        ++page;
    lv_obj_scroll_to_y(host, 0, LV_ANIM_OFF);
    ui_alarms_show(host, language, clear_action, acknowledge_action);
}
static void stamp(char *out, size_t size, uint32_t epoch) {
    if (!epoch) {
        snprintf(out, size, "--");
        return;
    }
    time_t value = epoch;
    struct tm date;
    localtime_r(&value, &date);
    strftime(out, size, "%Y-%m-%d\n%H:%M:%S", &date);
}
void ui_alarms_show(lv_obj_t *parent, bool english, lv_event_cb_t clear_history, lv_event_cb_t acknowledge) {
    host = parent;
    language = english;
    clear_action = clear_history;
    acknowledge_action = acknowledge;
    lv_obj_clean(host);
    size_t count = storage_alarm_history(entries, ALARM_HISTORY_LIMIT);
    size_t pages = ui_alarm_page_count(count);
    if (page >= pages)
        page = pages - 1;
    lv_obj_t *previous = ui_button(host, tr("이전", "Previous"), 0, 0, 108, 38, paging, -1);
    lv_obj_t *next = ui_button(host, tr("다음", "Next"), 116, 0, 108, 38, paging, 1);
    lv_obj_set_disabled(previous, page == 0);
    lv_obj_set_disabled(next, page + 1 >= pages);
    char text[120];
    snprintf(text, sizeof(text), "%u / %u  (%u)", (unsigned)page + 1, (unsigned)pages, (unsigned)count);
    ui_label(host, text, 238, 10, 180, &ui_font_small);
    lv_obj_t *clear = ui_button(host, tr("이력 삭제", "Delete history"), 424, 0, 132, 38, clear_history, 0);
    lv_obj_set_disabled(clear, count == 0);
    uint32_t active, acknowledged;
    bool fresh = storage_alarm_status(esp_timer_get_time() / 1000, &active, &acknowledged);
    lv_obj_t *ack = ui_button(host, tr("경보해제", "Acknowledge"), 566, 0, 144, 38, acknowledge, 0);
    lv_obj_set_disabled(ack, !fresh || !(active & ~acknowledged));
    int y = 50;
    uint32_t visible = active & ~acknowledged;
    if (!fresh) {
        ui_label(host, tr("통신 끊김 · 현재 경보 확인 대기", "Offline - awaiting live alarm status"), 0, y,
                 710, &ui_font_body);
        y += 34;
    } else if (!visible) {
        ui_label(host,
                 active ? tr("알림 해제 · 재조회 대기", "Acknowledged - awaiting next controller frame")
                        : tr("현재 경보 없음", "No active alarms"),
                 0, y, 710, &ui_font_body);
        y += 34;
    } else {
        unsigned index = 0;
        for (unsigned bit = 0; bit < 16; ++bit) {
            if (!(visible & (1u << bit)))
                continue;
            snprintf(text, sizeof(text), "%s · %s", tr("발생", "ACTIVE"),
                     language ? names_en[bit] : names_ko[bit]);
            lv_obj_t *label =
                ui_label(host, text, (index % 2) * 356, y + (index / 2) * 30, 350, &ui_font_body);
            lv_obj_set_style_text_color(label, lv_color_hex(0xcc4949), 0);
            ++index;
        }
        y += ((index + 1) / 2) * 30 + 6;
    }
    if (!count) {
        ui_label(host, tr("경보 이력 없음", "No alarm history"), 0, y, 710, &ui_font_body);
        return;
    }
    size_t first = page * ALARM_PAGE_SIZE;
    size_t rows = count - first;
    if (rows > ALARM_PAGE_SIZE)
        rows = ALARM_PAGE_SIZE;
    lv_obj_t *table = lv_table_create(host);
    lv_obj_set_pos(table, 0, y);
    lv_obj_set_width(table, 710);
    lv_obj_set_style_text_font(table, &ui_font_small, 0);
    lv_table_set_column_count(table, 4);
    lv_table_set_row_count(table, rows + 1);
    const int widths[] = {185, 175, 145, 195};
    const char *heads_ko[] = {"발생시간", "경보", "상태", "해제시간"};
    const char *heads_en[] = {"Raised", "Alarm", "Status", "Cleared"};
    for (unsigned col = 0; col < 4; ++col) {
        lv_table_set_column_width(table, col, widths[col]);
        lv_table_set_cell_value(table, 0, col, language ? heads_en[col] : heads_ko[col]);
    }
    for (size_t i = 0; i < rows; ++i) {
        const alarm_history_entry_t *entry = &entries[first + i];
        stamp(text, sizeof(text), entry->raised_at);
        lv_table_set_cell_value(table, i + 1, 0, text);
        lv_table_set_cell_value(table, i + 1, 1, language ? names_en[entry->alarm] : names_ko[entry->alarm]);
        lv_table_set_cell_value(table, i + 1, 2,
                                entry->cleared ? tr("해제", "Cleared")
                                : fresh        ? tr("발생", "Active")
                                               : tr("확인 대기", "Unconfirmed"));
        stamp(text, sizeof(text), entry->cleared_at);
        lv_table_set_cell_value(table, i + 1, 3, text);
    }
}
