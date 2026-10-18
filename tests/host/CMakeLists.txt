cmake_minimum_required(VERSION 3.22)
project(monitor_host C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(MONITOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${MONITOR_ROOT}/cmake/monitor_sources.cmake")
set(LVROOT "${MONITOR_ROOT}/managed_components/lvgl__lvgl")
file(GLOB_RECURSE LV_SOURCES "${LVROOT}/src/*.c")
add_library(lvgl STATIC ${LV_SOURCES})
target_include_directories(lvgl PUBLIC "${LVROOT}" "${LVROOT}/include" "${MONITOR_ROOT}/include")
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1
  LV_CONF_PATH="${MONITOR_ROOT}/include/lv_conf.h")
add_executable(monitor_preview preview.c storage_fake.c
  ${MONITOR_UI_SOURCES} ${MONITOR_DOMAIN_SOURCES} ${MONITOR_SYNC_SOURCES} "${MONITOR_ROOT}/src/services/storage.c")
target_include_directories(monitor_preview PRIVATE include "${MONITOR_ROOT}/include" "${MONITOR_ROOT}/main/ui")
target_link_libraries(monitor_preview PRIVATE lvgl m)
target_compile_options(monitor_preview PRIVATE -Wall -Wextra)
add_executable(storage_check storage_check.c storage_fake.c
  "${MONITOR_ROOT}/src/services/storage.c" "${MONITOR_ROOT}/src/domain/model.c")
target_include_directories(storage_check PRIVATE include "${MONITOR_ROOT}/include")
add_executable(settings_sync_check settings_sync_check.c storage_fake.c
  ${MONITOR_SYNC_SOURCES} "${MONITOR_ROOT}/src/services/storage.c" "${MONITOR_ROOT}/src/domain/model.c")
target_include_directories(settings_sync_check PRIVATE include "${MONITOR_ROOT}/include")
target_compile_options(settings_sync_check PRIVATE -Wall -Wextra -Werror)
add_executable(clock_check clock_check.c storage_fake.c "${MONITOR_ROOT}/src/board/clock.c"
  "${MONITOR_ROOT}/src/domain/clock.c")
target_include_directories(clock_check PRIVATE include "${MONITOR_ROOT}/include")
target_compile_definitions(clock_check PRIVATE settimeofday=host_settimeofday)
target_compile_options(clock_check PRIVATE -Wall -Wextra -Werror)

add_executable(touch_check touch_check.c "${MONITOR_ROOT}/src/board/touch_transform.c")
target_include_directories(touch_check PRIVATE "${MONITOR_ROOT}/include")
target_link_libraries(touch_check PRIVATE m)
foreach(mode IN ITEMS calibration fixed)
  add_executable(touch_${mode}_check touch_preview.c
    "${MONITOR_ROOT}/src/board/touch_calibration.c" "${MONITOR_ROOT}/src/board/touch_transform.c")
  target_include_directories(touch_${mode}_check BEFORE PRIVATE "${mode}")
  target_include_directories(touch_${mode}_check PRIVATE include "${MONITOR_ROOT}/include")
  target_link_libraries(touch_${mode}_check PRIVATE lvgl m)
  target_compile_options(touch_${mode}_check PRIVATE -Wall -Wextra -Werror)
endforeach()

add_executable(alarm_check alarm_check.c storage_fake.c
  "${MONITOR_ROOT}/src/services/storage.c" "${MONITOR_ROOT}/src/domain/model.c")
target_include_directories(alarm_check PRIVATE include "${MONITOR_ROOT}/include")
target_compile_options(alarm_check PRIVATE -Wall -Wextra -Werror)
