#include "ui/widgets.h"
#include "fonts.h"
void ui_txt(lv_obj_t *o, const char *s) {
    lv_label_set_text(o, s);
}
void ui_hide(lv_obj_t *o, bool h) {
    lv_obj_set_hidden(o, h);
}
lv_obj_t *ui_label(lv_obj_t *p, const char *s, int x, int y, int w, const lv_font_t *font) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0x193047), 0);
    ui_txt(o, s);
    return o;
}
lv_obj_t *ui_box(lv_obj_t *p, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 12, 0);
    return o;
}
lv_obj_t *ui_button(lv_obj_t *p, const char *s, int x, int y, int w, int h, lv_event_cb_t cb, intptr_t data) {
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xe8f1f5), 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t *l = ui_label(b, s, 0, 0, w, &ui_font_body);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    if (cb)
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)data);
    return b;
}
void ui_nav_label(lv_obj_t *b, const char *s) {
    for (unsigned i = 0; i < lv_obj_get_child_count(b); i++) {
        lv_obj_t *l = lv_obj_get_child(b, i);
        if (lv_obj_check_type(l, &lv_label_class))
            ui_txt(l, s);
    }
}
