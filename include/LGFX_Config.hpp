#pragma once

// Compatibility entry point for board settings imported from eez-tft-test.
// This ESP-IDF project uses esp_lcd for the 800x480 RGB panel. There is no
// LovyanGFX LGFX class or C3/ST7789 SPI driver in this firmware.
// C and C++ clients share the actual board configuration below.
#include "board/board_config.h"
