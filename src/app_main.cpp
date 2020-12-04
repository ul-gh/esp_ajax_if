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
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include "network_setup.hpp"
#include "api_server.hpp"
#include "app_hw_control.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "app_main";

const NetworkConfig net_conf;

constexpr unsigned long serial_baudrate = 115200;

// Without name resolution, Windows and browser clients spam the server with
// failing DNS queries. Also this is used by the WiFi Manager and portal page.
DNSServer dns_server;

// ESPAsyncWebserver must be one single instance
AsyncWebServer http_backend{net_conf.http_tcp_port};

// HTTP server provides REST API + HTML5 AJAX web interface on port 80
APIServer api_server{&http_backend};

// PS-PWM hardware controller instance registers into the AJAX API server
PsPwmAppHwControl ps_pwm_controller{&api_server};

void update_debug_messages();
void print_debug_messages();

void setup() {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    Serial.begin(serial_baudrate);
    //setup_wifi_station(); // Optional, when not using AsyncWifiManager
    setup_wifi_hostap(net_conf); // Optional, when not using AsyncWifiManager
    http_backend.begin(); // Needed when not using AsyncWifiManager!
    setup_dns_server(dns_server, net_conf);
    // Disable above setup_wifi... calls when using AsyncWifiManager
    //run_wifi_manager(&dns_server, &http_backend);
    // Run HTTP server and prepare AJAX API registration
    api_server.begin();
    /** Begin operation of PWM stages etc.
     * This also starts the timer callbacks etc.
     * This will fail if networking etc. is not set up correctly!
     */
    ps_pwm_controller.begin();
}

void loop() {
    dns_server.processNextRequest();
    update_debug_messages();
    delay(50);
}


void update_debug_messages(){
    static unsigned int loopctr;
    loopctr++;
    if (loopctr > 100) {
        loopctr = 0;
        //heap_trace_start(HEAP_TRACE_LEAKS);
        print_debug_messages();
    }
}

void print_debug_messages(){
    ESP_LOGD(TAG,
             "\nFree Heap: %d   Minimum ever free heap: %d"
             "\nSSE number of clients: %d   SSE average queue length: %d",
             ESP.getFreeHeap(), ESP.getMinFreeHeap(),
             api_server.event_source->count(), api_server.event_source->avgPacketsWaiting()
             );
    //ESP_LOGD(TAG, "\nWifi stations connected: %d", WiFi.softAPgetStationNum());
    //if (!heap_caps_check_integrity_all(true)) {
    //    ESP_LOGE(TAG, "Heap integrity check failed!");
    //    abort();
    //}
}
