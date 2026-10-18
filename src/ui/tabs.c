#include "ui/tabs.h"

void ui_tab_checked(lv_obj_t *button, bool checked) {
    if (checked)
        lv_obj_add_state(button, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(button, LV_STATE_CHECKED);
    for (uint32_t i = 0; i < lv_obj_get_child_count(button); i++) {
        lv_obj_t *label = lv_obj_get_child(button, i);
        if (lv_obj_check_type(label, &lv_label_class)) {
            lv_obj_set_style_pad_all(label, 0, 0);
            lv_obj_set_height(label, LV_SIZE_CONTENT);
            lv_obj_set_align(label, LV_ALIGN_CENTER);
            lv_obj_set_pos(label, 0, 0);
            lv_obj_set_style_text_color(label, checked ? lv_color_white() : lv_color_hex(0x193047), 0);
        }
    }
}

void ui_tabs_select(lv_obj_t *const *buttons, size_t count, size_t selected) {
    for (size_t i = 0; i < count; i++)
        ui_tab_checked(buttons[i], i == selected);
}
