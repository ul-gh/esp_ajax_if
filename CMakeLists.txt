cmake_minimum_required(VERSION 3.16.0)

set(EXTRA_COMPONENT_DIRS
    "components/esp-rainmaker/components"
    )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(extra_includes
    "components/esp_idf_build_incompatible/ArduinoJson/src"
    )
message(STATUS
        "=========================== Extra Includes: ${extra_includes}"
        )
include_directories("${extra_includes}")
link_directories("${extra_includes}")

project(esp_ajax_if)
