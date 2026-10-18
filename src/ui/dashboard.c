#include "ui/dashboard.h"
#include "ui/widgets.h"
#include "ui/charts.h"
#include "ui/tabs.h"
#include "ui/alarms.h"
#include "actions.h"
#include "ui.h"
#include "screens.h"
#include "fonts.h"
#include "services/service.h"
#include "services/storage.h"
#include "services/settings_sync.h"
#include "board/board.h"
#include "ui/animations.h"
#include "domain/settings_catalog.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static mex_snapshot_t snap;
static int page = 1, settings_group, eq_tab, history_range;
static bool history_table;
static lv_obj_t *chips[5][5], *icons[5], *percents[5], *chip_hosts[5];
static lv_obj_t *main_chart, *history_chart, *history_body, *equipment_body, *settings_body, *popup, *entry,
    *entry_error, *dim, *wake;
static lv_chart_series_t *main_series[4], *history_series[4];
static lv_obj_t *toast, *equipment_rows[27], *equipment_meta, *manual_buttons[16];
static lv_obj_t *group_buttons[6], *equipment_tabs[4], *history_tabs[4], *history_toggle, *gauges[5],
    *computed[5];
static uint32_t toast_until;
static const setting_field_t *editing;
static monitor_config_t *cfg;
static trend_point_t live[900];
// UI-task-only scratch memory. Renderers copy values into LVGL before calling
// another renderer; these arrays must never be retained or used by other tasks.
static union {
    trend_point_t points[180];
} ui_scratch;
static size_t live_next, live_count;
static uint64_t last_second, last_animation;
static int64_t last_schedule = -1, history_minute = -1;
static uint32_t last_sequence;
static bool sleeping;
static uint32_t colors[] = {0x0e9482, 0x188bd5, 0xeb783a, 0x715cd5, 0x17a0ad};
static const char *coil_ko[] = {"송풍",      "냉방 1",    "냉방 2",    "난방 1", "난방 2", "난방 3",
                                "난방 4",    "가습 1",    "가습 2",    "급수",   "배수",   "냉방 운전",
                                "난방 운전", "가습 운전", "제습 운전", "냉방 3"};
static const char *coil_en[] = {"Fan",        "Cooling 1",    "Cooling 2",   "Heating 1",
                                "Heating 2",  "Heating 3",    "Heating 4",   "Humidify 1",
                                "Humidify 2", "Water supply", "Water drain", "Cooling",
                                "Heating",    "Humidifying",  "Drying",      "Cooling 3"};
static const char *tr(const char *ko, const char *en) {
    return cfg->language ? en : ko;
}
static void refresh_settings(void);
static void refresh_equipment(void);
static void refresh_history(void);
static void translate(void);
static void notice(const char *s) {
    if (!toast) {
        toast = ui_box(lv_layer_top(), 160, 10, 480, 38);
        lv_obj_set_style_bg_color(toast, lv_color_hex(0x193047), 0);
        ui_label(toast, "", 12, 8, 456, &ui_font_small);
        lv_obj_set_style_text_color(lv_obj_get_child(toast, 0), lv_color_white(), 0);
    }
    ui_txt(lv_obj_get_child(toast, 0), s);
    ui_hide(toast, false);
    toast_until = lv_tick_get() + 4000;
}
static void close_popup(lv_event_t *e) {
    (void)e;
    if (popup) {
        lv_obj_delete(popup);
        popup = NULL;
    }
    entry = NULL;
    entry_error = NULL;
    editing = NULL;
}
static void modal(const char *title) {
    close_popup(NULL);
    popup = ui_box(lv_layer_top(), 0, 0, 800, 480);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x193047), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_60, 0);
    lv_obj_t *card = ui_box(popup, 130, 36, 540, 408);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    ui_label(card, title, 20, 18, 500, &ui_font_title);
    ui_button(card, tr("취소", "Cancel"), 20, 348, 150, 44, close_popup, 0);
}
static void confirm_command(lv_event_t *e) {
    intptr_t data = (intptr_t)lv_event_get_user_data(e);
    uint8_t fc = data >= 100 ? 5 : 6;
    uint16_t address = data >= 100 ? (data - 100) / 2 + 1 : 40005;
    int value = data >= 100 ? (data - 100) % 2 : (int)data;
    bool ok = service_send(fc, address, value, fc == 5);
    close_popup(NULL);
    notice(ok ? tr("전송 중 · 재조회로 확인합니다", "Sending · verifying readback")
              : tr("요청 거부 · 연결 및 운전 상태 확인", "Rejected · check connection/state"));
}
static void manual(lv_event_t *e) {
    int coil = (intptr_t)lv_event_get_user_data(e);
    service_snapshot(&snap);
    if (!snap.online || mex_running(&snap)) {
        notice(tr("연결된 컨트롤러를 정지한 후 사용하세요", "Stop the connected controller first"));
        return;
    }
    int value = !(snap.coils & (1 << coil));
    modal(tr("수동 출력 확인", "Confirm manual output"));
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    char s[120];
    snprintf(s, sizeof(s), "%s  →  %s", cfg->language ? coil_en[coil] : coil_ko[coil],
             value ? tr("운전", "ON") : tr("정지", "OFF"));
    ui_label(card, s, 20, 84, 490, &ui_font_title);
    ui_label(card, tr("실제 장비의 출력 상태가 변경됩니다.", "This changes the physical equipment output."),
             20, 136, 495, &ui_font_body);
    ui_button(card, tr("적용", "Apply"), 350, 348, 170, 44, confirm_command, 100 + coil * 2 + value);
}
void action_power(lv_event_t *e) {
    (void)e;
    service_snapshot(&snap);
    if (!snap.online) {
        notice(tr("통신 연결 확인 필요", "Controller offline"));
        return;
    }
    bool run = mex_running(&snap);
    modal(run ? tr("장비 정지", "Stop equipment") : tr("장비 운전", "Start equipment"));
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    ui_label(card, tr("컨트롤러 운전 상태를 변경합니다.", "Change the controller run state."), 20, 90, 500,
             &ui_font_body);
    ui_button(card, tr("적용", "Apply"), 350, 348, 170, 44, confirm_command, !run);
}
static void navigate(int p) {
    close_popup(NULL);
    page = p;
    loadScreen((enum ScreensEnum)p);
    if (p == 2)
        refresh_history();
    if (p == 3)
        refresh_equipment();
    if (p == 4)
        refresh_settings();
}
void action_nav_main(lv_event_t *e) {
    (void)e;
    navigate(1);
}
void action_nav_history(lv_event_t *e) {
    (void)e;
    navigate(2);
}
void action_nav_equipment(lv_event_t *e) {
    (void)e;
    navigate(3);
}
void action_nav_settings(lv_event_t *e) {
    (void)e;
    navigate(4);
}
void action_shortcut(lv_event_t *e) {
    (void)e;
    if (cfg->shortcut == 0) {
        settings_group = 0;
        navigate(4);
    } else {
        eq_tab = cfg->shortcut == 1 ? 0 : 1;
        navigate(3);
    }
}
static int local_get(const char *key) {
    if (!strncmp(key, "quick", 5))
        return cfg->quick[atoi(key + 5)];
    if (!strncmp(key, "mode", 4))
        return cfg->mode[atoi(key + 4)];
    if (!strcmp(key, "brightness"))
        return cfg->brightness;
    if (!strcmp(key, "sleep"))
        return cfg->sleep_minutes;
    if (!strcmp(key, "language"))
        return cfg->language;
    if (!strcmp(key, "unit"))
        return cfg->unit;
    if (!strcmp(key, "shortcut"))
        return cfg->shortcut;
    if (!strncmp(key, "on", 2) || !strncmp(key, "off", 3)) {
        bool on = key[1] == 'n';
        int v = on ? cfg->schedule_on[atoi(key + 2)] : cfg->schedule_off[atoi(key + 3)];
        return v / 60 * 100 + v % 60;
    }
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    if (!board_clock_valid())
        t = (struct tm){.tm_year = 126, .tm_mon = 0, .tm_mday = 1};
    if (!strcmp(key, "year"))
        return t.tm_year + 1900;
    if (!strcmp(key, "month"))
        return t.tm_mon + 1;
    if (!strcmp(key, "day"))
        return t.tm_mday;
    if (!strcmp(key, "hour"))
        return t.tm_hour;
    if (!strcmp(key, "minute"))
        return t.tm_min;
    return 0;
}
static bool local_set(const char *key, int v) {
    monitor_config_t before = *cfg;
    if (!strncmp(key, "quick", 5))
        cfg->quick[atoi(key + 5)] = v;
    else if (!strncmp(key, "mode", 4))
        cfg->mode[atoi(key + 4)] = v;
    else if (!strcmp(key, "brightness"))
        cfg->brightness = v;
    else if (!strcmp(key, "sleep"))
        cfg->sleep_minutes = v;
    else if (!strcmp(key, "language"))
        cfg->language = v;
    else if (!strcmp(key, "unit"))
        cfg->unit = v;
    else if (!strcmp(key, "shortcut"))
        cfg->shortcut = v;
    else if (!strncmp(key, "on", 2) || !strncmp(key, "off", 3)) {
        if (v % 100 > 59)
            return false;
        bool on = key[1] == 'n';
        if (on)
            cfg->schedule_on[atoi(key + 2)] = v / 100 * 60 + v % 100;
        else
            cfg->schedule_off[atoi(key + 3)] = v / 100 * 60 + v % 100;
        last_schedule = -1;
    } else {
        time_t now = time(NULL);
        struct tm d;
        localtime_r(&now, &d);
        if (!board_clock_valid())
            d = (struct tm){.tm_year = 126, .tm_mon = 0, .tm_mday = 1};
        if (!strcmp(key, "year"))
            d.tm_year = v - 1900;
        else if (!strcmp(key, "month"))
            d.tm_mon = v - 1;
        else if (!strcmp(key, "day"))
            d.tm_mday = v;
        else if (!strcmp(key, "hour"))
            d.tm_hour = v;
        else if (!strcmp(key, "minute"))
            d.tm_min = v;
        else
            return false;
        d.tm_sec = 0;
        d.tm_isdst = 0;
        last_schedule = -1;
        return board_clock_set(&d);
    }
    if (!storage_save()) {
        *cfg = before;
        return false;
    }
    if (cfg->unit != before.unit)
        service_set_unit(cfg->unit);
    translate();
    return true;
}
static void save_entry(lv_event_t *e) {
    (void)e;
    char *end;
    double value = strtod(lv_textarea_get_text(entry), &end);
    double raw = value * editing->scale;
    if (end == lv_textarea_get_text(entry) || *end || !isfinite(raw) || fabs(raw - round(raw)) > .001 ||
        raw < editing->minimum || raw > editing->maximum) {
        ui_txt(entry_error, tr("입력 범위를 확인하세요", "Check range and precision"));
        return;
    }
    bool deferred = settings_sync_supported(editing->address);
    bool ok = deferred           ? settings_sync_stage(editing->address, (int)round(raw))
              : editing->address ? service_send(6, editing->address, (int)round(raw), editing->stopped)
                                 : local_set(editing->local, (int)round(raw));
    if (!ok) {
        ui_txt(entry_error, deferred ? tr("저장 실패", "Storage error - value was not saved")
                                     : tr("적용 실패 · 값 또는 장비 상태 확인",
                                          "Failed · check value and controller state"));
        return;
    }
    bool remote = editing->address != 0;
    close_popup(NULL);
    refresh_settings();
    notice(deferred ? tr("저장됨", "Saved - awaiting controller readback")
           : remote ? tr("전송 중 · 재조회로 확인합니다", "Sending · verifying readback")
                    : tr("저장됨", "Saved"));
}
static void edit_field(lv_event_t *e) {
    size_t i = (intptr_t)lv_event_get_user_data(e);
    if (i >= SETTING_COUNT)
        return;
    const setting_field_t *f = &setting_fields[i];
    if (!strcmp(f->local, "unsupported")) {
        notice(
            tr("통신 주소 미확인 · 실제 적용 미지원", "Address unverified · controller write unavailable"));
        return;
    }
    service_snapshot(&snap);
    if (f->address && !settings_sync_supported(f->address) &&
        (!snap.online || (f->stopped && mex_running(&snap)))) {
        notice(tr("연결 및 장비 정지 상태 확인 필요", "Check connection and stopped state"));
        return;
    }
    modal(cfg->language ? f->en : f->ko);
    editing = f;
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    char s[100];
    snprintf(s, sizeof(s), "%s %.1f ~ %.1f", tr("범위", "Range"), (double)f->minimum / f->scale,
             (double)f->maximum / f->scale);
    ui_label(card, s, 20, 56, 500, &ui_font_small);
    if (!strncmp(f->local, "mode", 4))
        ui_label(card,
                 tr("0: 단계  1: 비례 표시 · 실제 출력 설정 아님", "0: Stages  1: Proportional display only"),
                 20, 78, 500, &ui_font_small);
    entry = lv_textarea_create(card);
    lv_obj_set_pos(entry, 20, 106);
    lv_obj_set_size(entry, 500, 46);
    lv_textarea_set_one_line(entry, true);
    lv_textarea_set_accepted_chars(entry, "0123456789.-");
    lv_textarea_set_max_length(entry, 9);
    lv_obj_set_style_text_font(entry, &ui_font_body, 0);
    int value = f->address ? (int16_t)snap.regs[f->address - 40000] : local_get(f->local);
    bool pending;
    settings_sync_value(f->address, &snap, &value, &pending);
    if (f->scale == 10)
        snprintf(s, sizeof(s), "%.1f", value / 10.0);
    else
        snprintf(s, sizeof(s), "%d", value);
    lv_textarea_set_text(entry, s);
    entry_error = ui_label(card, "", 20, 158, 500, &ui_font_small);
    lv_obj_t *kb = lv_keyboard_create(card);
    lv_obj_set_align(kb, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(kb, 20, 186);
    lv_obj_set_size(kb, 500, 152);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, entry);
    lv_obj_add_event_cb(kb, save_entry, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, close_popup, LV_EVENT_CANCEL, NULL);
    ui_button(card, tr("저장", "Save"), 350, 348, 170, 44, save_entry, 0);
}
void action_quick_temp(lv_event_t *e) {
    (void)e;
    notice(settings_sync_stage(40012, cfg->quick[0]) ? tr("저장됨", "Saved")
                                                     : tr("저장 실패", "Storage error"));
}
void action_quick_hum(lv_event_t *e) {
    (void)e;
    notice(settings_sync_stage(40013, cfg->quick[2]) ? tr("저장됨", "Saved")
                                                     : tr("저장 실패", "Storage error"));
}
static void preset(lv_event_t *e) {
    int n = (intptr_t)lv_event_get_user_data(e);
    bool ok = settings_sync_stage(n < 2 ? 40012 : 40013, cfg->quick[n]);
    notice(ok ? tr("저장됨", "Saved - applies when connected") : tr("저장 실패", "Storage error"));
}
void action_settings_tab(lv_event_t *e) {
    intptr_t selected = (intptr_t)lv_event_get_user_data(e);
    if (selected < 0 || selected >= 6)
        return;
    settings_group = selected;
    lv_obj_scroll_to_y(settings_body, 0, LV_ANIM_OFF);
    refresh_settings();
}
static void refresh_settings(void) {
    if (!settings_body)
        return;
    ui_tabs_select(group_buttons, 6, settings_group);
    lv_obj_clean(settings_body);
    int y = 0;
    service_snapshot(&snap);
    for (size_t i = 0; i < SETTING_COUNT; i++) {
        const setting_field_t *f = &setting_fields[i];
        if (f->group != settings_group)
            continue;
        lv_obj_t *b = ui_button(settings_body, "", 0, y, 710, 48, edit_field, i);
        lv_obj_clean(b);
        ui_label(b, cfg->language ? f->en : f->ko, 12, 13, 490, &ui_font_body);
        char s[100];
        bool unsupported = !strcmp(f->local, "unsupported");
        if (unsupported)
            snprintf(s, sizeof(s), "%s", tr("미지원", "Unavailable"));
        else if (f->address && !snap.online && !settings_sync_supported(f->address))
            snprintf(s, sizeof(s), "--");
        else {
            int v = f->address ? (int16_t)snap.regs[f->address - 40000] : local_get(f->local);
            bool pending = false;
            settings_sync_value(f->address, &snap, &v, &pending);
            if (!strncmp(f->local, "mode", 4))
                snprintf(s, sizeof(s), "%s", v ? tr("비례 표시", "Proportional") : tr("단계", "Stages"));
            else if (f->scale == 10)
                snprintf(s, sizeof(s), "%.1f", v / 10.0);
            else
                snprintf(s, sizeof(s), "%d", v);
            if (pending) {
                size_t length = strlen(s);
                snprintf(s + length, sizeof(s) - length, " *");
            }
        }
        lv_obj_t *l = ui_label(b, s, 510, 14, 184, &ui_font_body);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(unsupported ? 0x8b98a5 : 0x0e9482), 0);
        y += 56;
    }
    if (settings_group == 0) {
        ui_label(settings_body,
                 tr("* 전송 중 · 재조회로 확인합니다", "* Saved locally - awaiting controller readback"), 0,
                 y, 710, &ui_font_small);
        y += 32;
        for (int i = 0; i < 4; i++) {
            char s[90];
            snprintf(s, sizeof(s), "%s %d: %.1f",
                     i < 2 ? tr("빠른 온도", "Quick temperature") : tr("빠른 습도", "Quick humidity"),
                     i % 2 + 1, cfg->quick[i] / 10.0);
            ui_button(settings_body, s, 0, y, 710, 44, preset, i);
            y += 52;
        }
    }
    if (settings_group == 4)
        ui_label(settings_body,
                 tr("같은 운전/정지 시각: 비활성 · 모니터가 켜져 있어야 실행",
                    "Equal times disable schedule. Monitor must remain powered."),
                 0, y, 710, &ui_font_small);
}
void action_equipment_tab(lv_event_t *e) {
    intptr_t selected = (intptr_t)lv_event_get_user_data(e);
    if (selected < 0 || selected >= 4)
        return;
    eq_tab = selected;
    lv_obj_scroll_to_y(equipment_body, 0, LV_ANIM_OFF);
    refresh_equipment();
}
static void alarm_confirm(lv_event_t *e) {
    bool deleting = (intptr_t)lv_event_get_user_data(e) != 0;
    bool ok =
        deleting ? storage_alarm_clear_history() : storage_alarm_acknowledge(esp_timer_get_time() / 1000);
    close_popup(NULL);
    if (deleting && ok)
        ui_alarms_first_page();
    refresh_equipment();
    notice(ok ? (deleting ? tr("이력 삭제 완료", "History deleted")
                          : tr("알림 해제 · 다음 응답에서 재확인", "Acknowledged - rechecking next frame"))
              : tr("처리 실패 · 상태 확인", "Failed - check status"));
}
static void alarm_ask(lv_event_t *e) {
    (void)e;
    modal(tr("경보해제", "Acknowledge alarms"));
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    ui_label(
        card,
        tr("모니터의 알림을 해제합니다.\n실제 경보가 계속되면 다음 통신 응답에서\n다시 표시됩니다.",
           "Acknowledge monitor notifications.\nPersisting alarms return on the next\ncontroller response."),
        20, 90, 500, &ui_font_body);
    ui_button(card, tr("적용", "Apply"), 350, 348, 170, 44, alarm_confirm, 0);
}
static void ask_alarm_delete(lv_event_t *e) {
    (void)e;
    // Reuse the same confirmation handler with an explicit local action.
    modal(tr("경보 이력 삭제", "Delete alarm history"));
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    ui_label(card,
             tr("선택 장비의 경보 이력을 삭제합니다.\n현재 경보 상태는 유지됩니다.",
                "Delete history for this unit.\nCurrent alarms remain unchanged."),
             20, 90, 500, &ui_font_body);
    ui_button(card, tr("삭제", "Delete"), 350, 348, 170, 44, alarm_confirm, 1);
}
static void reset_hours(lv_event_t *e) {
    (void)e;
    memset(cfg->runtime_s, 0, sizeof(cfg->runtime_s));
    storage_save();
    close_popup(NULL);
    refresh_equipment();
    notice(tr("모니터 적산 초기화됨", "Monitor runtime counters reset"));
}
static void ask_reset(lv_event_t *e) {
    (void)e;
    modal(tr("모니터 적산 초기화", "Reset monitor runtime counters"));
    lv_obj_t *card = lv_obj_get_child(popup, 0);
    ui_label(card,
             tr("컨트롤러 전극봉 적산은 변경되지 않습니다.", "Controller cylinder runtime is not changed."),
             20, 90, 500, &ui_font_body);
    ui_button(card, tr("삭제", "Reset"), 350, 348, 170, 44, reset_hours, 0);
}
static void refresh_equipment(void) {
    if (!equipment_body)
        return;
    ui_tabs_select(equipment_tabs, 4, eq_tab);
    lv_obj_clean(equipment_body);
    memset(equipment_rows, 0, sizeof(equipment_rows));
    memset(manual_buttons, 0, sizeof(manual_buttons));
    equipment_meta = NULL;
    char s[250];
    service_snapshot(&snap);
    int y = 0;
    if (eq_tab == 0) {
        snprintf(s, sizeof(s), "%s %u   |   %s %.1f A   |   DI 0x%04X", tr("장비", "Unit"), snap.regs[20],
                 tr("현재전류", "Current"), snap.regs[11] / 10.0, snap.regs[2]);
        equipment_meta = ui_label(equipment_body, s, 0, 0, 710, &ui_font_body);
        y = 36;
        for (int i = 0; i < 16; i++) {
            snprintf(s, sizeof(s), "%-16s   %s   |   %llu h %02llu m",
                     cfg->language ? coil_en[i] : coil_ko[i],
                     !snap.online ? "--" : (snap.coils & (1 << i) ? tr("운전", "ON") : tr("정지", "OFF")),
                     (unsigned long long)cfg->runtime_s[i] / 3600,
                     (unsigned long long)(cfg->runtime_s[i] / 60) % 60);
            equipment_rows[i] = ui_label(equipment_body, s, 8, y, 690, &ui_font_body);
            y += 34;
        }
        for (int i = 0; i < 11; i++) {
            int bit = i < 8 ? 8 + i : i - 8;
            snprintf(s, sizeof(s), "DI %02d   %s", i + 1,
                     !snap.online ? "--" : (snap.regs[2] & (1 << bit) ? "ON" : "OFF"));
            equipment_rows[16 + i] = ui_label(equipment_body, s, 8, y, 690, &ui_font_body);
            y += 30;
        }
        ui_button(equipment_body, tr("모니터 적산 초기화", "Reset monitor runtime"), 0, y + 10, 710, 44,
                  ask_reset, 0);
    } else if (eq_tab == 1) {
        ui_alarms_show(equipment_body, cfg->language, ask_alarm_delete, alarm_ask);
    } else if (eq_tab == 2) {
        ui_label(equipment_body,
                 tr("예약은 모니터에서 실행 · 같은 시각은 비활성",
                    "Schedule runs on monitor · equal times disable"),
                 0, 0, 710, &ui_font_body);
        y = 40;
        const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (int i = 0; i < 7; i++) {
            int on = cfg->schedule_on[i], off = cfg->schedule_off[i];
            snprintf(s, sizeof(s), "%s     %02d:%02d → %02d:%02d     %s", days[i], on / 60, on % 60, off / 60,
                     off % 60, on == off ? tr("비활성", "Disabled") : tr("사용", "Enabled"));
            ui_label(equipment_body, s, 0, y, 710, &ui_font_body);
            y += 34;
        }
    } else {
        ui_label(equipment_body,
                 tr("정지 상태에서만 수동 제어 · 실제 출력 변경",
                    "Manual outputs require stopped state · physical control"),
                 0, 0, 710, &ui_font_small);
        y = 36;
        for (int i = 0; i < 16; i++) {
            snprintf(s, sizeof(s), "%s · %s", cfg->language ? coil_en[i] : coil_ko[i],
                     !snap.online            ? "--"
                     : snap.coils & (1 << i) ? "ON"
                                             : "OFF");
            lv_obj_t *b = ui_button(equipment_body, s, (i % 2) * 360, y + (i / 2) * 52, 348, 44, manual, i);
            manual_buttons[i] = b;
            if (!snap.online || mex_running(&snap))
                lv_obj_add_state(b, LV_STATE_DISABLED);
        }
    }
}
static size_t live_points(trend_point_t *out) {
    size_t n = 0;
    for (size_t i = 0; i < live_count; i += 5)
        out[n++] = live[(live_next + 900 - live_count + i) % 900];
    return n;
}
void action_history_tab(lv_event_t *e) {
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 4)
        return;
    if (i == 4)
        history_table = !history_table;
    else
        history_range = i;
    refresh_history();
}
static void refresh_history(void) {
    if (!history_body)
        return;
    ui_tabs_select(history_tabs, 4, history_range);
    ui_tab_checked(history_toggle, history_table);
    lv_obj_clean(history_body);
    history_chart = NULL;
    trend_point_t *points = ui_scratch.points;
    size_t n;
    if (history_range == 0)
        n = live_points(points);
    else {
        int seconds = history_range == 1 ? 3600 : history_range == 2 ? 86400 : 604800;
        n = storage_trend(points, 180, time(NULL) - seconds);
    }
    if (history_table) {
        lv_obj_t *table = lv_table_create(history_body);
        lv_obj_set_width(table, 710);
        lv_obj_set_style_text_font(table, &ui_font_small, 0);
        lv_table_set_column_count(table, 5);
        lv_table_set_row_count(table, n + 1);
        const char *heads[] = {"Time", "Temp C", "RH %", "T min/max", "RH min/max"};
        for (int k = 0; k < 5; k++) {
            lv_table_set_column_width(table, k, k ? 130 : 180);
            lv_table_set_cell_value(table, 0, k, heads[k]);
        }
        for (size_t i = 0; i < n; i++) {
            char s[48];
            time_t epoch = points[i].timestamp;
            struct tm t;
            localtime_r(&epoch, &t);
            strftime(s, sizeof(s), "%Y-%m-%d\n%H:%M:%S", &t);
            lv_table_set_cell_value(table, i + 1, 0, epoch ? s : "--");
            snprintf(s, sizeof(s), "%.1f", points[i].temp / 10.0);
            lv_table_set_cell_value(table, i + 1, 1, points[i].samples ? s : "--");
            snprintf(s, sizeof(s), "%.1f", points[i].hum / 10.0);
            lv_table_set_cell_value(table, i + 1, 2, points[i].samples ? s : "--");
            snprintf(s, sizeof(s), "%.1f / %.1f", points[i].temp_min / 10.0, points[i].temp_max / 10.0);
            lv_table_set_cell_value(table, i + 1, 3, points[i].samples ? s : "--");
            snprintf(s, sizeof(s), "%.1f / %.1f", points[i].hum_min / 10.0, points[i].hum_max / 10.0);
            lv_table_set_cell_value(table, i + 1, 4, points[i].samples ? s : "--");
        }
    } else {
        ui_label(history_body,
                 tr("온도 -25~85℃ · 습도 0~100% · 연한 선: 설정값",
                    "Temp -25..85 C · RH 0..100% · pale lines: setpoints"),
                 0, 0, 710, &ui_font_small);
        lv_obj_t *host = ui_box(history_body, 0, 26, 710, 224);
        history_chart = ui_chart(host, history_series, 710, 224);
        ui_fill_chart(history_chart, history_series, points, n);
        char s[150];
        if (n) {
            char a[24], b[24];
            time_t ta = points[0].timestamp, tb = points[n - 1].timestamp;
            struct tm t;
            localtime_r(&ta, &t);
            strftime(a, sizeof(a), "%Y-%m-%d %H:%M", &t);
            localtime_r(&tb, &t);
            strftime(b, sizeof(b), "%Y-%m-%d %H:%M", &t);
            snprintf(s, sizeof(s), "%s — %s  |  %s %u", a, b, tr("표시", "Points"), (unsigned)n);
        } else
            snprintf(s, sizeof(s), "%s",
                     tr("기록 대기 · RTC 시간과 통신 상태 확인",
                        "Waiting for records · check RTC and connection"));
        ui_label(history_body, s, 0, 256, 710, &ui_font_small);
    }
}
static void translate(void) {
    lv_obj_t *titles[] = {objects.main_title, objects.history_title, objects.equipment_title,
                          objects.settings_title};
    if (history_toggle)
        ui_nav_label(history_toggle, tr("그래프 / 표", "Chart / Table"));
    const char *hk[] = {"15분", "1시간", "24시간", "7일"}, *he[] = {"15m", "1h", "24h", "7d"};
    for (int i = 0; i < 4; i++)
        if (history_tabs[i])
            ui_nav_label(history_tabs[i], tr(hk[i], he[i]));
    const char *gk[] = {"기본", "고급", "미확인", "화면", "예약", "시계"},
               *ge[] = {"Basic", "Expert", "Unmapped", "Display", "Schedule", "Clock"};
    for (int i = 0; i < 6; i++)
        if (group_buttons[i])
            ui_nav_label(group_buttons[i], tr(gk[i], ge[i]));
    const char *ek[] = {"현재상태", "경보", "스케줄", "수동"},
               *ee[] = {"Status", "Alarms", "Schedule", "Manual"};
    for (int i = 0; i < 4; i++)
        if (equipment_tabs[i])
            ui_nav_label(equipment_tabs[i], tr(ek[i], ee[i]));
    const char *ko[] = {"환경 대시보드", "데이터 이력", "장비 상태", "운전 설정"},
               *en[] = {"Climate dashboard", "Live history", "Equipment", "Settings"};
    for (int i = 0; i < 4; i++)
        ui_txt(titles[i], tr(ko[i], en[i]));
    lv_obj_t *navs[][4] = {
        {objects.main_nav_0, objects.main_nav_1, objects.main_nav_2, objects.main_nav_3},
        {objects.history_nav_0, objects.history_nav_1, objects.history_nav_2, objects.history_nav_3},
        {objects.equipment_nav_0, objects.equipment_nav_1, objects.equipment_nav_2, objects.equipment_nav_3},
        {objects.settings_nav_0, objects.settings_nav_1, objects.settings_nav_2, objects.settings_nav_3}};
    const char *nk[] = {"대시보드", "이력", "장비", "설정"},
               *ne[] = {"Dashboard", "History", "Equipment", "Settings"};
    for (int p = 0; p < 4; p++)
        for (int i = 0; i < 4; i++)
            ui_nav_label(navs[p][i], tr(nk[i], ne[i]));
    ui_txt(objects.temperature_caption, tr("현재온도", "Temperature"));
    ui_txt(objects.humidity_caption, tr("현재습도", "Humidity"));
    ui_txt(objects.trend_title, tr("실시간 추세", "Live trend"));
    ui_txt(objects.trend_window, tr("최근 15분", "Last 15 min"));
    ui_nav_label(objects.shortcut_button, tr("바로가기", "Shortcut"));
    lv_obj_t *dt[] = {objects.device_title_0, objects.device_title_1, objects.device_title_2,
                      objects.device_title_3, objects.device_title_4};
    const char *dk[] = {"송풍", "냉방", "난방", "가습", "제습"},
               *de[] = {"Fan", "Cooling", "Heating", "Humidify", "Drying"};
    for (int i = 0; i < 5; i++)
        ui_txt(dt[i], tr(dk[i], de[i]));
}
static void wake_up(lv_event_t *e) {
    (void)e;
    sleeping = false;
    board_display_on(true);
    ui_hide(wake, true);
    lv_display_trigger_activity(NULL);
}
void dashboard_init(void) {
    cfg = storage_config();
    ui_init();
    translate();
    lv_obj_t *ii[] = {objects.device_icon_0, objects.device_icon_1, objects.device_icon_2,
                      objects.device_icon_3, objects.device_icon_4};
    lv_obj_t *pp[] = {objects.device_percent_0, objects.device_percent_1, objects.device_percent_2,
                      objects.device_percent_3, objects.device_percent_4};
    lv_obj_t *hh[] = {objects.device_chips_0, objects.device_chips_1, objects.device_chips_2,
                      objects.device_chips_3, objects.device_chips_4};
    const int counts[] = {1, 4, 5, 2, 4};
    for (int k = 0; k < 5; k++) {
        icons[k] = ii[k];
        percents[k] = pp[k];
        chip_hosts[k] = hh[k];
        lv_obj_clean(hh[k]);
        for (int j = 0; j < counts[k]; j++) {
            int w = counts[k] == 1 ? 70 : 22;
            chips[k][j] = ui_box(hh[k], counts[k] == 1 ? 29 : j * 25, 0, w, 24);
            lv_obj_set_style_radius(chips[k][j], 6, 0);
            char s[8];
            snprintf(s, sizeof(s), "%d", j + 1);
            lv_obj_t *l = ui_label(chips[k][j], s, 0, 4, w, &ui_font_small);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        }
    }
    lv_obj_t *cards[] = {objects.device_card_0, objects.device_card_1, objects.device_card_2,
                         objects.device_card_3, objects.device_card_4};
    for (int k = 1; k < 5; k++) {
        gauges[k] = lv_arc_create(cards[k]);
        lv_obj_set_pos(gauges[k], 30, 36);
        lv_obj_set_size(gauges[k], 84, 84);
        lv_arc_set_bg_angles(gauges[k], 140, 40);
        lv_arc_set_range(gauges[k], 0, 100);
        lv_obj_remove_style(gauges[k], NULL, LV_PART_KNOB);
        lv_obj_set_clickable(gauges[k], false);
        lv_obj_set_style_arc_width(gauges[k], 5, LV_PART_MAIN);
        lv_obj_set_style_arc_width(gauges[k], 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(gauges[k], lv_color_hex(colors[k]), LV_PART_INDICATOR);
        ui_hide(gauges[k], true);
        computed[k] = ui_label(cards[k], tr("계산값", "Calculated"), 8, 126, 128, &ui_font_small);
        lv_obj_set_style_text_align(computed[k], LV_TEXT_ALIGN_CENTER, 0);
        ui_hide(computed[k], true);
    }
    main_chart = ui_chart(objects.main_chart_host, main_series, 338, 74);
    // EEZ owns the toolbar and its CLICKED events; only body children are dynamic.
    lv_obj_t *history[] = {objects.history_tab_0, objects.history_tab_1, objects.history_tab_2,
                           objects.history_tab_3};
    lv_obj_t *equipment[] = {objects.equipment_tab_0, objects.equipment_tab_1, objects.equipment_tab_2,
                             objects.equipment_tab_3};
    lv_obj_t *settings[] = {objects.settings_tab_0, objects.settings_tab_1, objects.settings_tab_2,
                            objects.settings_tab_3, objects.settings_tab_4, objects.settings_tab_5};
    memcpy(history_tabs, history, sizeof(history));
    memcpy(equipment_tabs, equipment, sizeof(equipment));
    memcpy(group_buttons, settings, sizeof(settings));
    history_toggle = objects.history_tab_4;
    history_body = objects.history_body;
    equipment_body = objects.equipment_body;
    settings_body = objects.settings_body;
    lv_obj_set_clickable(objects.temperature_card, true);
    lv_obj_add_event_cb(objects.temperature_card, edit_field, LV_EVENT_CLICKED, (void *)0);
    lv_obj_set_clickable(objects.humidity_card, true);
    lv_obj_add_event_cb(objects.humidity_card, edit_field, LV_EVENT_CLICKED, (void *)1);
    dim = ui_box(lv_layer_top(), 0, 0, 800, 480);
    lv_obj_set_style_bg_color(dim, lv_color_black(), 0);
    lv_obj_set_clickable(dim, false);
    lv_obj_set_scrollable(dim, false);
    wake = ui_box(lv_layer_top(), 0, 0, 800, 480);
    lv_obj_set_style_bg_opa(wake, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(wake, wake_up, LV_EVENT_PRESSED, NULL);
    ui_hide(wake, true);
    translate();
    refresh_settings();
    refresh_equipment();
    refresh_history();
}
void dashboard_tick(void) {
    uint64_t now = esp_timer_get_time() / 1000;
    service_snapshot(&snap);
    settings_sync_tick(now, &snap);
    storage_alarm_observe(&snap, now, time(NULL), board_clock_valid());
    static uint32_t alarm_drawn_revision;
    if (page == 3 && eq_tab == 1 && !popup && alarm_drawn_revision != storage_alarm_revision()) {
        int32_t scroll = lv_obj_get_scroll_y(equipment_body);
        refresh_equipment();
        lv_obj_scroll_to_y(equipment_body, scroll, LV_ANIM_OFF);
        alarm_drawn_revision = storage_alarm_revision();
    }
    if (now - last_animation >= 100) {
        last_animation = now;
        static int f;
        f = (f + 1) % 12;
        for (int k = 0; k < 5; k++) {
            bool proportional = k && cfg->mode[k - 1];
            uint8_t stages = mex_stages(&snap, k);
            ui_hide(icons[k], proportional);
            ui_hide(percents[k], !proportional);
            ui_hide(chip_hosts[k], proportional);
            if (k) {
                ui_hide(gauges[k], !proportional);
                ui_hide(computed[k], !proportional);
                ui_txt(computed[k], tr("계산값", "Calculated"));
            }
            if (proportional) {
                int current = k <= 2 ? (int16_t)snap.regs[36] : snap.regs[37],
                    sp = snap.regs[k <= 2 ? 12 : 13], dev = snap.regs[13 + k];
                char s[24];
                snprintf(s, sizeof(s), "%d%%", mex_proportion(current, sp, dev, k == 2 || k == 3));
                ui_txt(percents[k], snap.online ? s : "--%");
                lv_arc_set_value(gauges[k],
                                 snap.online ? mex_proportion(current, sp, dev, k == 2 || k == 3) : 0);
            } else {
                lv_image_set_src(icons[k], monitor_frames[k][snap.online && stages ? f : 0]);
                lv_obj_set_style_image_opa(icons[k], snap.online && stages ? 255 : 80, 0);
                for (int j = 0; j < 5; j++)
                    if (chips[k][j]) {
                        bool on = snap.online && (stages & (1 << j));
                        uint32_t alarms = mex_alarms(&snap);
                        bool alarm = false;
                        if (snap.online) {
                            if (k == 0)
                                alarm = alarms & 1;
                            else if (k == 1 || k == 4)
                                alarm = j == 0   ? (alarms & 2)
                                        : j == 1 ? (alarms & 4)
                                        : j == 2 ? (alarms & (1 << 10))
                                                 : false;
                            else if (k == 2)
                                alarm = alarms & 8;
                            else if (k == 3)
                                alarm = alarms & 16;
                        }
                        lv_obj_set_style_bg_color(chips[k][j],
                                                  lv_color_hex(alarm ? 0xcc4949
                                                               : on  ? colors[k]
                                                                     : 0xe8edf2),
                                                  0);
                        lv_obj_set_style_text_color(lv_obj_get_child(chips[k][j], 0),
                                                    (on || alarm) ? lv_color_white() : lv_color_hex(0x708295),
                                                    0);
                    }
            }
        }
    }
    if (toast && lv_tick_get() > toast_until)
        ui_hide(toast, true);
    lv_obj_set_style_bg_opa(dim, (100 - cfg->brightness) * 2, 0);
    if (cfg->sleep_minutes && !sleeping &&
        lv_display_get_inactive_time(NULL) > (uint32_t)cfg->sleep_minutes * 60000) {
        sleeping = true;
        ui_hide(wake, false);
        lv_obj_move_foreground(wake);
        board_display_on(false);
    }
    if (now - last_second < 1000)
        return;
    last_second = now;
    time_t epoch = time(NULL);
    bool valid = board_clock_valid();
    struct tm t;
    localtime_r(&epoch, &t);
    storage_observe(&snap, now, epoch, valid);
    bool fresh = snap.online && now - snap.received_ms < 3000;
    int64_t minute = epoch / 60;
    if (valid && fresh) {
        int command = mex_schedule(last_schedule, minute, t.tm_wday, t.tm_hour * 60 + t.tm_min,
                                   cfg->schedule_on, cfg->schedule_off);
        if (command >= 0)
            service_send(6, 40005, command, false);
        last_schedule = minute;
    } else
        last_schedule = -1;
    char s[150];
    strftime(s, sizeof(s), "%Y-%m-%d %H:%M KST", &t);
    lv_obj_t *cl[] = {objects.main_clock, objects.history_clock, objects.equipment_clock,
                      objects.settings_clock};
    lv_obj_t *co[] = {objects.main_connection, objects.history_connection, objects.equipment_connection,
                      objects.settings_connection};
    for (int i = 0; i < 4; i++) {
        ui_txt(cl[i], valid ? s : tr("시간 미설정", "Set clock"));
        ui_txt(co[i], fresh ? tr("연결됨", "ONLINE") : tr("통신 끊김", "OFFLINE"));
        lv_obj_set_style_text_color(co[i], lv_color_hex(fresh ? 0x0e9482 : 0xcc4949), 0);
    }
    snprintf(s, sizeof(s), "%.1f", (int16_t)snap.regs[36] / 10.0);
    ui_txt(objects.temperature_value, snap.sequence ? s : "--.-");
    snprintf(s, sizeof(s), "%.1f", snap.regs[37] / 10.0);
    ui_txt(objects.humidity_value, snap.sequence ? s : "--.-");
    int setpoint;
    bool pending;
    settings_sync_value(40012, &snap, &setpoint, &pending);
    snprintf(s, sizeof(s), "%s %.1f ℃%s", tr("설정", "Set"), setpoint / 10.0, pending ? " *" : "");
    ui_txt(objects.temperature_set, s);
    settings_sync_value(40013, &snap, &setpoint, &pending);
    snprintf(s, sizeof(s), "%s %.1f %%%s", tr("설정", "Set"), setpoint / 10.0, pending ? " *" : "");
    ui_txt(objects.humidity_set, s);
    if (fresh)
        lv_obj_remove_state(objects.power_button, LV_STATE_DISABLED);
    else
        lv_obj_add_state(objects.power_button, LV_STATE_DISABLED);
    ui_nav_label(objects.power_button, mex_running(&snap) ? tr("운전 중 · 정지", "Running · Stop")
                                                          : tr("정지 · 운전", "Stopped · Start"));
    if (!fresh)
        snprintf(s, sizeof(s), "%s · %llu s", tr("마지막 수신", "Last received"),
                 (unsigned long long)(snap.sequence ? (now - snap.received_ms) / 1000 : 0));
    else if (mex_alarms(&snap))
        snprintf(s, sizeof(s), "%s · %s", tr("경보 발생", "Active alarm"),
                 tr("장비 화면에서 확인", "Open equipment"));
    else if (snap.regs[4] & 3)
        snprintf(s, sizeof(s), "%s · %u s",
                 snap.regs[4] & 1 ? tr("운전지연", "Start delay") : tr("정지지연", "Stop delay"),
                 snap.regs[5] >> 8);
    else
        snprintf(s, sizeof(s), "%s · %s", tr("정상 연결", "Connected"),
                 storage_healthy() ? tr("내장 기록 사용", "Flash logging")
                                   : tr("저장 실패", "Storage error"));
    ui_txt(objects.main_status, s);
    lv_obj_set_style_text_color(objects.main_status,
                                lv_color_hex(!fresh || mex_alarms(&snap) ? 0xcc4949 : 0x708295), 0);
    static command_state_t previous;
    static uint32_t previous_settings_revision;
    if (previous_settings_revision != settings_sync_revision()) {
        previous_settings_revision = settings_sync_revision();
        if (page == 4 && !popup)
            refresh_settings();
    }
    command_state_t result = service_result();
    if (result != previous) {
        previous = result;
        if (result >= CMD_APPLIED)
            notice(result == CMD_APPLIED    ? tr("재조회 확인 · 적용됨", "Readback confirmed")
                   : result == CMD_REJECTED ? tr("요청 거부", "Command rejected")
                                            : tr("적용 확인 불가 · 상태 재조회 필요",
                                                 "Outcome uncertain · inspect controller state"));
        if (page == 4 && !popup)
            refresh_settings();
    }
    bool sample = fresh && snap.sequence != last_sequence && !(mex_alarms(&snap) & (1u << 11));
    trend_point_t p = {.timestamp = valid ? epoch : 0,
                       .temp = (int16_t)snap.regs[36],
                       .hum = snap.regs[37],
                       .temp_min = (int16_t)snap.regs[36],
                       .temp_max = (int16_t)snap.regs[36],
                       .hum_min = snap.regs[37],
                       .hum_max = snap.regs[37],
                       .set_temp = snap.regs[12],
                       .set_hum = snap.regs[13],
                       .samples = sample};
    last_sequence = snap.sequence;
    live[live_next] = p;
    live_next = (live_next + 1) % 900;
    if (live_count < 900)
        live_count++;
    if ((now / 1000) % 5 == 0) {
        trend_point_t *points = ui_scratch.points;
        size_t n = live_points(points);
        ui_fill_chart(main_chart, main_series, points, n);
        if (page == 2 && history_range == 0 && !popup)
            refresh_history();
    }
    if (page == 2 && history_range && minute != history_minute) {
        history_minute = minute;
        refresh_history();
    }
    if (page == 3 && eq_tab == 0) {
        if (equipment_meta) {
            snprintf(s, sizeof(s), "%s %u | %s %.1f A | DI 0x%04X", tr("장비", "Unit"), snap.regs[20],
                     tr("현재전류", "Current"), snap.regs[11] / 10.0, snap.regs[2]);
            ui_txt(equipment_meta,
                   snap.online ? s : tr("통신 끊김 · 마지막 수신값", "Offline · last received values"));
        }
        for (int i = 0; i < 27; i++)
            if (equipment_rows[i]) {
                if (i < 16)
                    snprintf(s, sizeof(s), "%s   %s   |   %llu h %02llu m",
                             cfg->language ? coil_en[i] : coil_ko[i],
                             !snap.online            ? "--"
                             : snap.coils & (1 << i) ? tr("운전", "ON")
                                                     : tr("정지", "OFF"),
                             (unsigned long long)cfg->runtime_s[i] / 3600,
                             (unsigned long long)(cfg->runtime_s[i] / 60) % 60);
                else {
                    int n = i - 16, bit = n < 8 ? 8 + n : n - 8;
                    snprintf(s, sizeof(s), "DI %02d   %s", n + 1,
                             !snap.online                ? "--"
                             : snap.regs[2] & (1 << bit) ? "ON"
                                                         : "OFF");
                }
                ui_txt(equipment_rows[i], s);
            }
    }
    if (page == 3 && eq_tab == 3)
        for (int i = 0; i < 16; i++)
            if (manual_buttons[i]) {
                snprintf(s, sizeof(s), "%s · %s", cfg->language ? coil_en[i] : coil_ko[i],
                         !snap.online            ? "--"
                         : snap.coils & (1 << i) ? "ON"
                                                 : "OFF");
                ui_nav_label(manual_buttons[i], s);
                if (!snap.online || mex_running(&snap))
                    lv_obj_add_state(manual_buttons[i], LV_STATE_DISABLED);
                else
                    lv_obj_remove_state(manual_buttons[i], LV_STATE_DISABLED);
            }
    ui_tick();
}
