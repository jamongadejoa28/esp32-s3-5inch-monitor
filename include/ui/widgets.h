#pragma once
#include "lvgl.h"
#include <stdint.h>

void ui_txt(lv_obj_t *obj, const char *text);
void ui_hide(lv_obj_t *obj, bool hidden);
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, int x, int y, int width, const lv_font_t *font);
lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int width, int height);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, int x, int y, int width, int height,
                    lv_event_cb_t callback, intptr_t data);
void ui_nav_label(lv_obj_t *button, const char *text);
