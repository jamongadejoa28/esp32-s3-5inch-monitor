cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mex_touch_monitor)
idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_include_directories(${LVGL_LIB} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/lvgl__lvgl/include")
# Explicit path prevents another component's lv_conf.h from shadowing ours.
target_compile_definitions(${LVGL_LIB} PUBLIC
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h")
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
  message(FATAL_ERROR "Use ESP-IDF v6.1 (VS Code: ESP-IDF Select Current ESP-IDF Version)")
endif()
