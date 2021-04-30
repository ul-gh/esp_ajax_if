set(requires
    "arduino-esp32"
    "ESPAsyncWebServer"
    "AsyncTCP"
    # Needs shim...
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

idf_component_register(SRCS "${sources}"
                       INCLUDE_DIRS "config" "include"
                       REQUIRES "${requires}"
)

#target_compile_definitions(${COMPONENT_TARGET} PUBLIC -DESP32)
#target_compile_options(${COMPONENT_TARGET} PRIVATE -fno-rtti)
