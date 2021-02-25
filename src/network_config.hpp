#ifndef NETWORK_CONFIG_HPP__
#define NETWORK_CONFIG_HPP__
#include "IPAddress.h"

struct NetworkConfig {
    IPAddress ap_ip{192,168,4,1};
    IPAddress ap_gateway{192,168,4,1};
    IPAddress ap_subnet{255,255,255,0};

    const char* ap_ssid = "isocal";
    const char* ap_psk = "123FOO456";

    const char* sta_ssid = "K9m";
    const char* sta_psk = "1c185GFzFgk";

    const char* host_name = "isocal";
    const char* dns_domain = "isocal.lan";

    bool use_dns = false;
    bool use_mdns = false;

    uint16_t http_tcp_port = 80;

    // WiFi Manager can launch the ESP in access point mode and serve a
    // captive portal page for WiFi configuration.
    // This mostly makes sense for use as a WiFi client.
    bool use_wifi_manager = false;
};

#endif
