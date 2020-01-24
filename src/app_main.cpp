#include <Arduino.h>
#include "esp32-hal-log.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SPIFFS.h>

#include "wifi_setup.hpp"
#include "http_server.h"
#include "app_hw_control.hpp"

#define SERIAL_BAUD 115200

// DNSServer dnsServer;
// AsyncWebServer http_server(80);
// Server-Sent Events (SSE) enable push updates on clients
// AsyncEventSource events("/events");

// ISOCAL HTTP server provides REST API + HTML5 AJAX web interface on port 80
HTTPServer http_server{80};
// PS-PWM generator instance is remote-controlled by the HTTP server
PSPWMGen ps_pwm_generator{http_server};

void setup() {
    Serial.begin(SERIAL_BAUD);
    //setup_wifi_station();
    setup_wifi_hostap();
    Serial.println();

    if (!SPIFFS.begin(true)) {
        Serial.println("Error mounting SPI Flash File System");
        return;
    }

    // dnsServer.start(53, "*", WiFi.softAPIP());
    // attach AsyncEventSource
    // server.addHandler(&events);

    http_server.begin();
    esp_log_level_set("*", ESP_LOG_DEBUG);
}

void loop() {
    static uint32_t timer_ctr = 0;
    // dnsServer.processNextRequest();
    ++timer_ctr;
    // Once every second
    if (timer_ctr > 50*1) {
        timer_ctr = 0;
        http_server.event_source.send("OK", "heartbeat", millis());
    }
    if (http_server.reboot_requested) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
    delay(20);
}
