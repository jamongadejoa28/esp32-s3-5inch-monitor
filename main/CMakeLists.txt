set(MONITOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${MONITOR_ROOT}/cmake/monitor_sources.cmake")
idf_component_register(
  SRCS "main.c" ${MONITOR_UI_SOURCES} ${MONITOR_DOMAIN_SOURCES} ${MONITOR_SYNC_SOURCES}
       "../src/services/storage.c" "../src/services/service.c"
       "../src/board/board.c" "../src/board/clock.c" "../src/board/touch_transform.c" "../src/board/touch_calibration.c"
  INCLUDE_DIRS "../include" "ui"
  REQUIRES lvgl esp_lcd esp_driver_gpio esp_driver_i2c esp_driver_uart
           esp_timer nvs_flash esp_partition esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wframe-larger-than=2048 -fstack-usage)
