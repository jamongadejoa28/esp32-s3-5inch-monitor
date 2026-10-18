#pragma once
#include "nvs.h"
#include <stdint.h>
typedef void *i2c_master_dev_handle_t;
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *, size_t, int);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t *, size_t, uint8_t *, size_t,
                                      int);
