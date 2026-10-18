#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_nav_main(lv_event_t * e);
extern void action_nav_history(lv_event_t * e);
extern void action_nav_equipment(lv_event_t * e);
extern void action_nav_settings(lv_event_t * e);
extern void action_power(lv_event_t * e);
extern void action_shortcut(lv_event_t * e);
extern void action_quick_temp(lv_event_t * e);
extern void action_quick_hum(lv_event_t * e);
extern void action_history_tab(lv_event_t * e);
extern void action_equipment_tab(lv_event_t * e);
extern void action_settings_tab(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/