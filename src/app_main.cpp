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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include "esp32-hal-log.h"
#include "esp_spiffs.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "sdkconfig.h"
//#include <ESPAsyncWiFiManager.h>


#include "info_debug_error.h"
#include "wifi_setup.hpp"
#include "api_server.hpp"
#include "app_hw_control.hpp"

#include "adc_temp.hpp"
#include "esp_err.h"

// Debug
#include "ESPAsyncWebServer.h"
#include "esp_heap_trace.h"

volatile void *dbg_a;
volatile void *dbg_b;

//SemaphoreHandle_t g_request_lock = xSemaphoreCreateBinary();

constexpr unsigned long serial_baudrate = 115200;
// TCP socket port number
constexpr uint16_t tcp_port = 80;

// ESPAsyncWebserver must be one single instance
AsyncWebServer http_backend{tcp_port};
// DNS resolution for captive portal page etc.
DNSServer* dns_server;
// Wifi connection manager
//AsyncWiFiManager* wifi_manager;

// HTTP server provides REST API + HTML5 AJAX web interface on port 80
APIServer* api_server;

// PS-PWM hardware controller instance runs the HTTP AJAX API server
PsPwmAppHwControl* ps_pwm_controller;

void setup() {
    // FIXME: DEBUG
    //heap_trace_init_tohost();
    //xSemaphoreGive(g_request_lock);
    //esp_log_level_set("*", ESP_LOG_DEBUG);
    Serial.begin(serial_baudrate);
    //setup_wifi_station(); // Optional, when not using AsyncWifiManager
    setup_wifi_hostap(); // Optional, when not using AsyncWifiManager
    http_backend.begin(); // Needed when not using AsyncWifiManager!
    //delay(300);
    //dns_server = new DNSServer{};
    // Disable above setup_wifi... calls when using AsyncWifiManager
    //wifi_manager = new AsyncWiFiManager{&http_backend, dns_server};
    //wifi_manager->resetSettings(); // For Debug etc.
    //wifi_manager->startConfigPortal("Isocal_Access_Point"); // Debug or special
    //wifi_manager->autoConnect("Isocal_Access_Point");
    //wifi_manager->setConfigPortalTimeout(180);
    esp_log_level_set("*", ESP_LOG_DEBUG);
    delay(100);
    api_server = new APIServer{&http_backend};
    api_server->activate_events_on("/events");
    ps_pwm_controller = new PsPwmAppHwControl{api_server};
    api_server->activate_default_callbacks();
    size_t tot_bytes, used_bytes;
    esp_spiffs_info(NULL, &tot_bytes, &used_bytes);
    Serial.print("SPIFFS filesystem size in bytes: ");
    Serial.print(tot_bytes);
    Serial.print("   Used bytes: ");
    Serial.println(used_bytes);
    AdcTemp::adc_init_test_capabilities();
}

void loop() {
    static int loopctr;
    // Application runs asynchronously, you can do anything here.
    delay(5000);
    loopctr++;
    if (loopctr > 4) {
        //heap_trace_start(HEAP_TRACE_LEAKS);
    }
    if (loopctr > 8) {
        //heap_trace_stop();
    }
    if (!heap_caps_check_integrity_all(true)) {
        Serial.println("!!!!!!!!!! Heap integrity check failed");
        abort();
    }
    String debug_msg = "Free Heap: " + String(ESP.getFreeHeap());
    debug_msg += "  Minimum ever free heap: " + String(ESP.getMinFreeHeap());
    //debug_msg += "  SSE queue length: ";
    //debug_msg += api_server->event_source->avgPacketsWaiting();
    debug_msg += "\n Wifi stations connected: ";
    debug_msg += WiFi.softAPgetStationNum();
    Serial.println(debug_msg);
}
