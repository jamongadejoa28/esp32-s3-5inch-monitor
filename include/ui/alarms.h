#pragma once
#include "lvgl.h"
#include <stddef.h>

size_t ui_alarm_page_count(size_t incident_count);
void ui_alarms_show(lv_obj_t *parent, bool english, lv_event_cb_t clear_history, lv_event_cb_t acknowledge);
void ui_alarms_first_page(void);
