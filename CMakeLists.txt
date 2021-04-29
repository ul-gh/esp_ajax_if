cmake_minimum_required(VERSION 3.16.0)


set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" "components/esp-rainmaker/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(extra_includes "components/ArduinoJson/src" "components/arduino-esp32/cores/esp32")
message(STATUS "=========================== Extra Includes: ${extra_includes}")
include_directories("${extra_includes}")
link_directories("${extra_includes}")

project(esp_ajax_if)
