#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SPIFFS.h>

#include "http_server.h"
#include "app_hw_control.h"

#define SERIAL_BAUD 115200

const char *ap_ssid = "test_ssid";
const char *ap_psk = "123FOO456";
const char *hostName = "test_host";
//IPAddress local_IP(192,168,4,1);
//IPAddress gateway(192,168,4,254);
//IPAddress subnet(255,255,255,0);

// DNSServer dnsServer;
// AsyncWebServer http_server(80);
// Server-Sent Events (SSE) enable push updates on clients
// AsyncEventSource events("/events");

static void setup_wifi_hostap();
static void setup_wifi_sta_ap();


// ISOCAL HTTP server provides REST API + HTML5 AJAX web interface on port 80
HTTPServer http_server{80};

// PS-PWM generator instance is remote-controlled by the HTTP server
PSPWMGen ps_pwm_generator{http_server};

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println();

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error mounting SPI Flash File System");
        return;
    }

    setup_wifi_hostap();

    MDNS.begin(hostName);
    MDNS.addService("http", "tcp", 80);

    // dnsServer.start(53, "*", WiFi.softAPIP());
    // attach AsyncEventSource
    // server.addHandler(&events);

    http_server.begin();

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

/* Configure WiFi for Access Point Mode
 */
static void setup_wifi_sta_ap() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(hostName);
    WiFi.begin(ap_ssid, ap_psk);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("STA: Failed!\n");
        WiFi.disconnect(false);
        delay(1000);
        WiFi.begin(ap_ssid, ap_psk);
    }
}

/* Configure WiFi for combined Station and Access Point Mode
 */
static void setup_wifi_hostap() {
    //Serial.print("Setting soft-AP configuration ... ");
    //Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
    WiFi.softAP(ap_ssid, ap_psk);
    //WiFi.softAPsetHostname(hostName);
    Serial.print(
        "Access point running. IP address: " + WiFi.softAPIP().toString());
    Serial.println("");
}

