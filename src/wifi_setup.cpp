#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "info_debug_error.h"
#include "wifi_setup.hpp"
#include "wifi_config.hpp"

/* Configure WiFi for Access Point Mode
 */
void setup_wifi_hostap() {
    WiFi.mode(WIFI_AP);
    info_print_sv(
        "Setting soft-AP configuration ...",
        WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet) ? "Ready" : "Failed!");
    WiFi.softAPsetHostname(hostName);
    info_print_sv(
        "Setting soft-AP ...",
        WiFi.softAP(ap_ssid, ap_psk) ? "Ready" : "Failed!");
    info_print_sv("Soft-AP IP address =", WiFi.softAPIP());
    if (use_mdns) {
        MDNS.begin(hostName);
        MDNS.addService("http", "tcp", 80);
    }
}

/* Configure WiFi for Station Mode
 */
void setup_wifi_station() {
    WiFi.mode(WIFI_STA);
    // Connect to Wi-Fi network with SSID and password
    info_print_sv("(Re-)Connecting to SSID:", sta_ssid);
    WiFi.begin(sta_ssid, sta_psk);
}

enum WIFI_EVENT wifi_handle_state_change() {
    static bool was_connected = false;
    bool is_connected = WiFi.status() == WL_CONNECTED;
    if (is_connected && !was_connected) {
        #ifdef WIFI_INIT_DELAY
        delay(WIFI_INIT_DELAY);
        #endif
        info_print_sv("WiFi connected. IP address:", WiFi.localIP());
        if (use_mdns) {
            MDNS.begin(hostName);
            MDNS.addService("http", "tcp", 80);
        }
        was_connected = true;
        return NEW_CONNECTION;
    } else if (!is_connected && was_connected) {
        info_print("WiFi disconnected!");
        was_connected = false;
        return DISCONNECTION;
    } else {
        return NO_CHANGE;
    }
}