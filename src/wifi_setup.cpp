#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "info_debug_error.h"
#include "wifi_setup.hpp"
#include "wifi_config.hpp"

static DNSServer *dns_server = nullptr;

/* Configure WiFi for Access Point Mode
 */
void setup_wifi_hostap() {
    WiFi.mode(WIFI_AP);
    //info_print_sv(
    //    "Setting soft-AP configuration ...",
    //    WiFi.softAPConfig(conf_ap_ip, conf_ap_gateway, conf_ap_subnet) ? "Ready" : "Failed!");
    WiFi.softAPsetHostname(conf_host_name);
    info_print_sv(
        "Setting soft-AP ...",
        WiFi.softAP(conf_ap_ssid, conf_ap_psk) ? "Ready" : "Failed!");
    info_print_sv("Soft-AP IP address =", WiFi.softAPIP());
    if (conf_use_mdns) {
        MDNS.begin(conf_host_name);
        MDNS.addService("http", "tcp", 80);
    } else {
        dns_server = new DNSServer{};
        dns_server->start(53, conf_dns_domain, conf_ap_ip);
    }
}

void process_dns_requests() {
    if (dns_server) {
        dns_server->processNextRequest();
    }
}

/* Configure WiFi for Station Mode
 */
void setup_wifi_station() {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    // Connect to Wi-Fi network with SSID and password
    info_print_sv("(Re-)Connecting to SSID:", conf_sta_ssid);
    WiFi.setHostname(conf_host_name);
    WiFi.begin(conf_sta_ssid, conf_sta_psk);
}

enum WIFI_EVENT wifi_handle_state_change() {
    static bool was_connected = false;
    bool is_connected = WiFi.status() == WL_CONNECTED;
    if (is_connected && !was_connected) {
        #ifdef WIFI_INIT_DELAY
        delay(WIFI_INIT_DELAY);
        #endif
        info_print_sv("WiFi connected. IP address:", WiFi.localIP());
        if (conf_use_mdns) {
            MDNS.begin(conf_host_name);
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

