#pragma once
#include "lvgl.h"
#include "services/storage.h"

lv_obj_t *ui_chart(lv_obj_t *host, lv_chart_series_t **series, int width, int height);
void ui_fill_chart(lv_obj_t *chart, lv_chart_series_t **series, const trend_point_t *points, size_t count);
