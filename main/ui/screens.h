#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_HISTORY = 2,
    SCREEN_ID_EQUIPMENT = 3,
    SCREEN_ID_SETTINGS = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *history;
    lv_obj_t *equipment;
    lv_obj_t *settings;
    lv_obj_t *main_title;
    lv_obj_t *main_clock;
    lv_obj_t *main_connection;
    lv_obj_t *main_nav_0;
    lv_obj_t *obj0;
    lv_obj_t *main_nav_1;
    lv_obj_t *obj1;
    lv_obj_t *main_nav_2;
    lv_obj_t *obj2;
    lv_obj_t *main_nav_3;
    lv_obj_t *obj3;
    lv_obj_t *temperature_card;
    lv_obj_t *temperature_caption;
    lv_obj_t *temperature_value;
    lv_obj_t *temperature_set;
    lv_obj_t *humidity_card;
    lv_obj_t *humidity_caption;
    lv_obj_t *humidity_value;
    lv_obj_t *humidity_set;
    lv_obj_t *trend_card;
    lv_obj_t *trend_title;
    lv_obj_t *trend_window;
    lv_obj_t *main_chart_host;
    lv_obj_t *device_card_0;
    lv_obj_t *device_title_0;
    lv_obj_t *device_icon_0;
    lv_obj_t *device_percent_0;
    lv_obj_t *device_chips_0;
    lv_obj_t *preview_chip_0_0;
    lv_obj_t *device_card_1;
    lv_obj_t *device_title_1;
    lv_obj_t *device_icon_1;
    lv_obj_t *device_percent_1;
    lv_obj_t *device_chips_1;
    lv_obj_t *preview_chip_1_0;
    lv_obj_t *preview_chip_1_1;
    lv_obj_t *preview_chip_1_2;
    lv_obj_t *preview_chip_1_3;
    lv_obj_t *device_card_2;
    lv_obj_t *device_title_2;
    lv_obj_t *device_icon_2;
    lv_obj_t *device_percent_2;
    lv_obj_t *device_chips_2;
    lv_obj_t *preview_chip_2_0;
    lv_obj_t *preview_chip_2_1;
    lv_obj_t *preview_chip_2_2;
    lv_obj_t *preview_chip_2_3;
    lv_obj_t *preview_chip_2_4;
    lv_obj_t *device_card_3;
    lv_obj_t *device_title_3;
    lv_obj_t *device_icon_3;
    lv_obj_t *device_percent_3;
    lv_obj_t *device_chips_3;
    lv_obj_t *preview_chip_3_0;
    lv_obj_t *preview_chip_3_1;
    lv_obj_t *device_card_4;
    lv_obj_t *device_title_4;
    lv_obj_t *device_icon_4;
    lv_obj_t *device_percent_4;
    lv_obj_t *device_chips_4;
    lv_obj_t *preview_chip_4_0;
    lv_obj_t *preview_chip_4_1;
    lv_obj_t *preview_chip_4_2;
    lv_obj_t *preview_chip_4_3;
    lv_obj_t *main_status;
    lv_obj_t *shortcut_button;
    lv_obj_t *obj4;
    lv_obj_t *power_button;
    lv_obj_t *obj5;
    lv_obj_t *history_title;
    lv_obj_t *history_clock;
    lv_obj_t *history_connection;
    lv_obj_t *history_nav_0;
    lv_obj_t *obj6;
    lv_obj_t *history_nav_1;
    lv_obj_t *obj7;
    lv_obj_t *history_nav_2;
    lv_obj_t *obj8;
    lv_obj_t *history_nav_3;
    lv_obj_t *obj9;
    lv_obj_t *history_content;
    lv_obj_t *history_body;
    lv_obj_t *preview_history_row_0;
    lv_obj_t *preview_history_row_1;
    lv_obj_t *history_tab_0;
    lv_obj_t *history_tab_1;
    lv_obj_t *history_tab_2;
    lv_obj_t *history_tab_3;
    lv_obj_t *history_tab_4;
    lv_obj_t *equipment_title;
    lv_obj_t *equipment_clock;
    lv_obj_t *equipment_connection;
    lv_obj_t *equipment_nav_0;
    lv_obj_t *obj10;
    lv_obj_t *equipment_nav_1;
    lv_obj_t *obj11;
    lv_obj_t *equipment_nav_2;
    lv_obj_t *obj12;
    lv_obj_t *equipment_nav_3;
    lv_obj_t *obj13;
    lv_obj_t *equipment_content;
    lv_obj_t *equipment_body;
    lv_obj_t *preview_equipment_row_0;
    lv_obj_t *preview_equipment_row_1;
    lv_obj_t *preview_equipment_row_2;
    lv_obj_t *preview_equipment_row_3;
    lv_obj_t *preview_equipment_row_4;
    lv_obj_t *equipment_tab_0;
    lv_obj_t *equipment_tab_1;
    lv_obj_t *equipment_tab_2;
    lv_obj_t *equipment_tab_3;
    lv_obj_t *settings_title;
    lv_obj_t *settings_clock;
    lv_obj_t *settings_connection;
    lv_obj_t *settings_nav_0;
    lv_obj_t *obj14;
    lv_obj_t *settings_nav_1;
    lv_obj_t *obj15;
    lv_obj_t *settings_nav_2;
    lv_obj_t *obj16;
    lv_obj_t *settings_nav_3;
    lv_obj_t *obj17;
    lv_obj_t *settings_content;
    lv_obj_t *settings_body;
    lv_obj_t *preview_settings_row_0;
    lv_obj_t *preview_settings_row_1;
    lv_obj_t *preview_settings_row_2;
    lv_obj_t *preview_settings_row_3;
    lv_obj_t *preview_settings_row_4;
    lv_obj_t *settings_tab_0;
    lv_obj_t *settings_tab_1;
    lv_obj_t *settings_tab_2;
    lv_obj_t *settings_tab_3;
    lv_obj_t *settings_tab_4;
    lv_obj_t *settings_tab_5;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_history();
void tick_screen_history();

void create_screen_equipment();
void tick_screen_equipment();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/