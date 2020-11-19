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

#include "../test/dummy_text.h"

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

void update_debug_messages();
void print_debug_messages();

void setup() {
    // FIXME: DEBUG
    //heap_trace_init_tohost();
    esp_log_level_set("*", ESP_LOG_DEBUG);
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
    delay(100);
    api_server = new APIServer{&http_backend};
    ps_pwm_controller = new PsPwmAppHwControl{api_server};
    size_t tot_bytes, used_bytes;
    esp_spiffs_info(NULL, &tot_bytes, &used_bytes);
    Serial.print("SPIFFS filesystem size in bytes: ");
    Serial.print(tot_bytes);
    Serial.print("   Used bytes: ");
    Serial.println(used_bytes);
    AdcTemp::adc_init_test_capabilities();
    http_backend.on("/sse_2", HTTP_GET, [](AsyncWebServerRequest *request) {
            Serial.println("Test");
            api_server->event_source->send(text_2kb, "sse_test");
            request->send(200, "text/plain", "2k Text sent");
    });
    http_backend.on("/sse_4", HTTP_GET, [](AsyncWebServerRequest *request) {
            Serial.println("Test");
            api_server->event_source->send(text_4kb, "sse_test");
            request->send(200, "text/plain", "4k Text sent");
    });
    http_backend.on("/sse_8", HTTP_GET, [](AsyncWebServerRequest *request) {
            Serial.println("Test");
            api_server->event_source->send(text_8kb, "sse_test");
            request->send(200, "text/plain", "8k Text sent");
    });
    http_backend.on("/sse_16", HTTP_GET, [](AsyncWebServerRequest *request) {
            Serial.println("Test");
            api_server->event_source->send(text_16kb, "sse_test");
            request->send(200, "text/plain", "16k Text sent");
    });
}

void loop() {
    delay(100);
    update_debug_messages();
}


void update_debug_messages(){
    static unsigned int loopctr;
    loopctr++;
    if (loopctr > 40) {
        loopctr = 0;
        //heap_trace_start(HEAP_TRACE_LEAKS);
        print_debug_messages();
    }
}

void print_debug_messages(){
    String debug_msg;
    debug_msg += "Free Heap: " + String(ESP.getFreeHeap());
    debug_msg += "  Minimum ever free heap: " + String(ESP.getMinFreeHeap());
    //debug_msg += "  SSE queue length: ";
    //debug_msg += api_server->event_source->avgPacketsWaiting();
    //debug_msg += "\n Wifi stations connected: " + WiFi.softAPgetStationNum();
    Serial.println(debug_msg);
    //if (!heap_caps_check_integrity_all(true)) {
    //    Serial.println("Heap integrity check failed!");
    //    abort();
    //}
}
