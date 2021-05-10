set(requires
    "arduino-esp32"
    "ESPAsyncWebServer"
    #"AsyncTCP" # Already required by ESPAsyncWebServer
    # Not compatible with IDF 4 build, see include_dirs below
    #"ArduinoJson"
    "esp32_ps_pwm"
)

set(sources
    "app_main.cpp"
    "app_state_model.cpp"
    "app_controller.cpp"
    "wifi_configurator.cpp"
    "api_server.cpp"
    "aux_hw_drv.cpp"
    "sensor_kty81_1xx.cpp"
    "esp32_adc_channel.cpp"
    "fs_io.cpp"
)

set(include_dirs
    "include"
    "config"
    "${PROJECT_DIR}/components/esp_idf_build_incompatible/ArduinoJson/src"
)

idf_component_register(
    SRCS "${sources}"
    INCLUDE_DIRS "${include_dirs}"
    REQUIRES "${requires}"
)

#target_compile_definitions(${COMPONENT_TARGET} PUBLIC -DESP32)
#target_compile_options(${COMPONENT_TARGET} PRIVATE xyz)

#message(STATUS "XXXXXXXXX COMPONENT_TARGET: ${COMPONENT_TARGET}")

