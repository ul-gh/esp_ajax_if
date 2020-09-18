#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sdkconfig.h"
#include <Arduino.h>
#include "esp32-hal-log.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <ESPAsyncWiFiManager.h>

#include "info_debug_error.h"
#include "wifi_setup.hpp"
#include "api_server.hpp"
#include "app_hw_control.hpp"

#include "adc_temp.hpp"

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
    api_server = new APIServer{&http_backend};
    esp_log_level_set("*", ESP_LOG_DEBUG);
    delay(100);
    ps_pwm_controller = new PsPwmAppHwControl{api_server};
    api_server->activate_events_on("/events");
    api_server->activate_default_callbacks();
    AdcTemp::adc_init_test_capabilities();
}

void loop() {
    // Application runs asynchronously, you can do anything here.
    delay(5000);
    String debug_msg = "Free Heap: " + String(ESP.getFreeHeap());
    debug_msg += "  SSE queue length: ";
    debug_msg += api_server->event_source->avgPacketsWaiting();
    debug_msg += "\n Wifi stations connected: ";
    debug_msg += WiFi.softAPgetStationNum();
    Serial.println(debug_msg);
}
