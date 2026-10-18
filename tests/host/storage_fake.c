#include "nvs.h"
#include "esp_partition.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
static uint8_t flash[1024 * 1024];
static struct {
    char key[32];
    uint8_t data[512];
    size_t length;
} nv[128];
static bool fail_nv;
void host_nvs_fail(bool fail) {
    fail_nv = fail;
}
static int slot(const char *key, bool create) {
    for (int i = 0; i < 128; ++i)
        if (nv[i].length && !strcmp(nv[i].key, key))
            return i;
    if (create)
        for (int i = 0; i < 128; ++i)
            if (!nv[i].length) {
                snprintf(nv[i].key, sizeof(nv[i].key), "%s", key);
                return i;
            }
    return -1;
}
static int initialized;
int nvs_flash_init(void) {
    if (!initialized) {
        memset(flash, 255, sizeof(flash));
        initialized = 1;
    }
    return 0;
}
int nvs_open(const char *s, int m, nvs_handle_t *h) {
    (void)s;
    (void)m;
    *h = 1;
    return 0;
}
int nvs_get_blob(nvs_handle_t h, const char *s, void *out, size_t *n) {
    (void)h;
    int i = slot(s, false);
    if (i < 0 || *n < nv[i].length)
        return -1;
    memcpy(out, nv[i].data, nv[i].length);
    *n = nv[i].length;
    return 0;
}
int nvs_set_blob(nvs_handle_t h, const char *s, const void *p, size_t n) {
    (void)h;
    int i = slot(s, true);
    if (fail_nv || i < 0)
        return -1;
    assert(n <= sizeof(nv[i].data));
    memcpy(nv[i].data, p, n);
    nv[i].length = n;
    return 0;
}
int nvs_commit(nvs_handle_t h) {
    (void)h;
    return 0;
}
const esp_partition_t *esp_partition_find_first(int t, int st, const char *n) {
    (void)t;
    (void)st;
    (void)n;
    static esp_partition_t p = {sizeof(flash)};
    return &p;
}
int esp_partition_read(const esp_partition_t *p, size_t o, void *d, size_t n) {
    assert(o + n <= p->size);
    memcpy(d, flash + o, n);
    return 0;
}
int esp_partition_write(const esp_partition_t *p, size_t o, const void *d, size_t n) {
    assert(o + n <= p->size);
    for (size_t i = 0; i < n; i++) {
        uint8_t v = ((uint8_t *)d)[i];
        assert((flash[o + i] & v) == v);
        flash[o + i] = v;
    }
    return 0;
}
int esp_partition_erase_range(const esp_partition_t *p, size_t o, size_t n) {
    assert(o + n <= p->size);
    assert(o % 4096 == 0 && n % 4096 == 0);
    memset(flash + o, 255, n);
    return 0;
}
void host_corrupt(size_t o) {
    flash[o] ^= 0x01;
}
