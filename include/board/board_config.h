#pragma once

/* Waveshare ESP32-S3-Touch-LCD-5, 800x480 revision.
 * Native ESP-IDF RGB peripheral; the copied C3/ST7789 SPI wiring does not apply.
 * Source: Waveshare 5-inch schematic and official ESP-IDF LVGL9 demo.
 */
#define BOARD_LCD_WIDTH 800
#define BOARD_LCD_HEIGHT 480
#define BOARD_LCD_PCLK_HZ 16000000
#define BOARD_LCD_HSYNC_PULSE 4
#define BOARD_LCD_HSYNC_BACK_PORCH 8
#define BOARD_LCD_HSYNC_FRONT_PORCH 8
#define BOARD_LCD_VSYNC_PULSE 4
#define BOARD_LCD_VSYNC_BACK_PORCH 8
#define BOARD_LCD_VSYNC_FRONT_PORCH 8
#define BOARD_LCD_HSYNC_GPIO 46
#define BOARD_LCD_VSYNC_GPIO 3
#define BOARD_LCD_DE_GPIO 5
#define BOARD_LCD_PCLK_GPIO 7
#define BOARD_LCD_DISP_GPIO (-1)
/* RGB565 bus order: B0..B4, G0..G5, R0..R4. */
#define BOARD_LCD_DATA_GPIOS {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40}
#define BOARD_LCD_BOUNCE_LINES 10
#define BOARD_LVGL_DRAW_LINES 40
#define BOARD_LCD_BYTES_PER_PIXEL 2

#define BOARD_I2C_PORT 0
#define BOARD_I2C_SDA_GPIO 8
#define BOARD_I2C_SCL_GPIO 9
#define BOARD_I2C_HZ 400000
#define BOARD_EXPANDER_MODE_ADDR 0x24
#define BOARD_EXPANDER_OUT_ADDR 0x38
#define BOARD_TOUCH_ADDR 0x5d
#define BOARD_TOUCH_INT_GPIO 4
#define BOARD_RTC_ADDR 0x51
#define BOARD_BACKLIGHT_MASK 0x04

/* Onboard auto-direction RS-485 transceiver: no software DE/RE pin. */
#define BOARD_RS485_UART 1
#define BOARD_RS485_TX_GPIO 44
#define BOARD_RS485_RX_GPIO 43
#define BOARD_RS485_BAUD 9600
