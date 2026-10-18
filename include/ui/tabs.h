#pragma once
#include "lvgl.h"
#include <stddef.h>

/* Radio selection: changing content cannot delete the EEZ-authored tab buttons. */
void ui_tabs_select(lv_obj_t *const *buttons, size_t count, size_t selected);
void ui_tab_checked(lv_obj_t *button, bool checked);
