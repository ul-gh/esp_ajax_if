cmake_minimum_required(VERSION 3.16.0)

# Project built successfully with std=C++20 before
set(CMAKE_CXX_STANDARD 20)

include_directories(components/arduino-esp32/tools/sdk/esp32/include/spiffs/include)

list(APPEND EXTRA_COMPONENT_DIRS
    "components/esp-rainmaker/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
 
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunsuffixed-float-constants")

message(STATUS "-------------- IDF_TARGET: ${IDF_TARGET}")
project(esp_ajax_if)

