#include "ui/charts.h"
lv_obj_t *ui_chart(lv_obj_t *host, lv_chart_series_t **series, int w, int h) {
    lv_obj_t *o = lv_chart_create(host);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 4, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_size(o, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_type(o, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(o, 180);
    lv_chart_set_axis_range(o, LV_CHART_AXIS_PRIMARY_Y, -250, 850);
    lv_chart_set_axis_range(o, LV_CHART_AXIS_SECONDARY_Y, 0, 1000);
    lv_chart_set_div_line_count(o, 4, 5);
    series[0] = lv_chart_add_series(o, lv_color_hex(0x188bd5), LV_CHART_AXIS_PRIMARY_Y);
    series[1] = lv_chart_add_series(o, lv_color_hex(0x0e9482), LV_CHART_AXIS_SECONDARY_Y);
    series[2] = lv_chart_add_series(o, lv_color_hex(0xb2d6ee), LV_CHART_AXIS_PRIMARY_Y);
    series[3] = lv_chart_add_series(o, lv_color_hex(0xb4dcd5), LV_CHART_AXIS_SECONDARY_Y);
    for (int i = 0; i < 4; i++)
        lv_chart_set_all_values(o, series[i], LV_CHART_POINT_NONE);
    return o;
}
void ui_fill_chart(lv_obj_t *o, lv_chart_series_t **series, const trend_point_t *p, size_t n) {
    unsigned count = n < 2 ? 2 : n;
    lv_chart_set_point_count(o, count);
    for (int j = 0; j < 4; j++)
        lv_chart_set_all_values(o, series[j], LV_CHART_POINT_NONE);
    for (size_t i = 0; i < n; i++) {
        if (!p[i].samples)
            continue;
        lv_chart_set_series_value_by_id(o, series[0], i, p[i].temp);
        lv_chart_set_series_value_by_id(o, series[1], i, p[i].hum);
        lv_chart_set_series_value_by_id(o, series[2], i, p[i].set_temp);
        lv_chart_set_series_value_by_id(o, series[3], i, p[i].set_hum);
    }
    lv_chart_refresh(o);
}
