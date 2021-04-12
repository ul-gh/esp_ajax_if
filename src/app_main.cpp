/** @file app_main.cpp
 * @brief ESP-AJAX-Lab
 * HTTP server, AJAX API backend, web application and Soc hardware drivers
 * for WiFi remote control of the MCPWM hardware modules on the
 * Espressif ESP32 SoC
 * 
 * Default configuration set up for generating a Phase-Shift-PWM waveform
 * between two pairs of hardware pins. This also features auxiliary
 * measurement and control functions:
 * 
 * - LUT-calibrated temperature sensor readout for KTY81-121 type silicon
 *   temperature sensors using the ESP32 ADC in its high-linearity region
 * - PWM reference signal generation for hardware overcurrent detector
 * - External GPIO output control for relays, fan, enable and error-reset
 * - TBD: Delta-Sigma conversion control and filter for insulated measurement
 *   of power stage current
 * 
 * License: GPL v.3 
 * U. Lukas 2020-09-30
 */
#include <Arduino.h>
#include "esp32-hal-log.h"
#include <ESPAsyncWebServer.h>

/** @brief Most configuration settings found here
 */
#include "app_config.hpp"

/** @brief Global application state model with default values.
 */
#include "app_state_model.hpp"

#include "wifi_configurator.hpp"
#include "api_server.hpp"
#include "app_controller.hpp"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "app_main";

void update_debug_messages();
void print_debug_messages();


// Global application state store object.
// Configuration and state stored/restored persistently in NVS and SPI flash.
auto state = AppState{};

// Without name resolution, Windows and browser clients spam the server with
// failing DNS queries. Also this is used by the WiFi Manager and portal page.
auto dns_server = DNSServer{};

// ESPAsyncWebserver must be one single instance
auto http_backend = AsyncWebServer{state.net_conf.http_tcp_port};

// HTTP server provides REST API + HTML5 AJAX web interface on port 80
auto api_server = APIServer{&http_backend};

auto wifi_configurator = WiFiConfigurator(state.net_conf, &http_backend, &dns_server);

// Application main controller.
//
// This registers the HTTP API callbacks, timer and interrupt handlers
// and runs the application event loop in a separate FreeRTOS task.
auto app_controller = AppController{state, &api_server};


void setup() {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    Serial.begin(SerialConfig::serial_baudrate);

    wifi_configurator.begin();

    http_backend.begin(); // Only when not using AsyncWifiManager!

    // Run HTTP server and prepare AJAX API registration
    api_server.begin();
    /** Begin operation of PWM stages etc.
     * This also starts the timer callbacks etc.
     * This will fail if networking etc. is not set up correctly!
     */
    app_controller.begin();
}

void loop() {
    if (state.net_conf.dns_active) {
        dns_server.processNextRequest();
    }
    update_debug_messages();
    delay(20);
}


void update_debug_messages(){
    static auto loopctr = 0u;
    loopctr++;
    if (loopctr > 500) {
        loopctr = 0;
        //heap_trace_start(HEAP_TRACE_LEAKS);
        print_debug_messages();
    }
}

void print_debug_messages(){
    //ESP_LOGD(TAG,
    //         "\nFree Heap: %d   Minimum ever free heap: %d"
    //         "\nSSE number of clients: %d   SSE average queue length: %d",
    //         ESP.getFreeHeap(), ESP.getMinFreeHeap(),
    //         api_server.event_source->count(), api_server.event_source->avgPacketsWaiting()
    //         );
    //ESP_LOGD(TAG, "\nWifi stations connected: %d", WiFi.softAPgetStationNum());
    //if (!heap_caps_check_integrity_all(true)) {
    //    ESP_LOGE(TAG, "Heap integrity check failed!");
    //    abort();
    //}
}
