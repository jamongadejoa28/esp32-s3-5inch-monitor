# Same application sources are compiled on the ESP32 and in the host UI checks.
set(MONITOR_DOMAIN_SOURCES
  "${MONITOR_ROOT}/src/domain/model.c"
  "${MONITOR_ROOT}/src/domain/clock.c"
  "${MONITOR_ROOT}/src/domain/settings_catalog.c")
set(MONITOR_SYNC_SOURCES "${MONITOR_ROOT}/src/services/settings_sync.c")
if(NOT CMAKE_SCRIPT_MODE_FILE)
  file(GLOB_RECURSE MONITOR_GENERATED_SOURCES CONFIGURE_DEPENDS "${MONITOR_ROOT}/main/ui/*.c")
  # EEZ 9.5 code generation uses supported v9.6 compatibility flag setters.
  # Keep deprecation diagnostics enabled in our own application sources.
  set_source_files_properties(${MONITOR_GENERATED_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()
set(MONITOR_UI_SOURCES
  "${MONITOR_ROOT}/src/ui/dashboard.c"
  "${MONITOR_ROOT}/src/ui/widgets.c"
  "${MONITOR_ROOT}/src/ui/charts.c"
  "${MONITOR_ROOT}/src/ui/tabs.c"
  "${MONITOR_ROOT}/src/ui/alarms.c"
  "${MONITOR_ROOT}/src/ui/animations.c"
  ${MONITOR_GENERATED_SOURCES})
