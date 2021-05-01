cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 20)

list(APPEND EXTRA_COMPONENT_DIRS
    "components/esp-rainmaker/components"
    )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

message(STATUS "-------------- IDF_TARGET: ${IDF_TARGET}")
project(esp_ajax_if)
