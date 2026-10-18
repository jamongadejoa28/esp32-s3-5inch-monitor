#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    int group;
    const char *ko, *en;
    int address, minimum, maximum, scale;
    const char *local;
    bool stopped;
} setting_field_t;
extern const setting_field_t setting_fields[];
extern const size_t setting_count;
#define SETTING_COUNT setting_count
