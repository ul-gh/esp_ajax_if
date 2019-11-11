#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>

#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "http_server.h"

#define GPIO_PWM0A_OUT 19 //Set GPIO 19 as PWM0A, first half-bridge
#define GPIO_PWM0B_OUT 18 //Set GPIO 18 as PWM0B, first half-bridge
#define GPIO_PWM1A_OUT 17 //Set GPIO 17 as PWM1A, second half-bridge
#define GPIO_PWM1B_OUT 16 //Set GPIO 16 as PWM1B, second half-bridge

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

static void do_ps_pwm();
static void setup_wifi_hostap();
static void setup_wifi_sta_ap();
//void httpRegisterCallbacks();

HTTPServer http_server(80);

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println();

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPI Flash File System");
        return;
    }

    setup_wifi_hostap();

    MDNS.begin(hostName);
    MDNS.addService("http", "tcp", 80);

    // dnsServer.start(53, "*", WiFi.softAPIP());
    // attach AsyncEventSource
    // server.addHandler(&events);
    
    http_server.begin();

    do_ps_pwm();
}

void loop() {
    // dnsServer.processNextRequest();
    delay(20);
    if (http_server.reboot_requested) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
}

static void do_ps_pwm() {
    /* Example settings:
     *
     * Frequency 20 kHz,
     * phase-shift duty cycle of 45% per output or 90% of rectified waveform,
     * dead-time values as indicated.
     */
    INFO("Configuring Phase-Shift-PWM...");
    pspwm_setup_gpios(MCPWM_UNIT_0,
                      GPIO_PWM0A_OUT, GPIO_PWM0B_OUT,
                      GPIO_PWM1A_OUT, GPIO_PWM1B_OUT);

    pspwm_init_individual_deadtimes(MCPWM_UNIT_0,
                                    20e3,
                                    0.45,
                                    500e-9, 1000e-9, 4000e-9, 2000e-9);
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

