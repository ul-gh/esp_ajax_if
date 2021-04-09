#ifndef NETWORK_CONFIG_HPP__
#define NETWORK_CONFIG_HPP__
#include "IPAddress.h"

struct NetworkConfig {
    // Run initially in access point mode when true
    bool ap_mode_active = true;

    IPAddress ip4_addr = {192,168,4,1};
    IPAddress ip4_gw = {192,168,4,1};
    IPAddress ip4_mask = {255,255,255,0};

    const char* ap_ssid = "isocal";
    const char* ap_psk = "123FOO456";

    const char* sta_ssid = "g7B";
    const char* sta_psk = "fH5mhq9Q2Ant";

    //const char* sta_ssid = "K9m";
    //const char* sta_psk = "1c185GFzFgk";

    // Hostname max length is 32 here.
    const char* hostname = "isocal";
    const char* dns_tld = ".lan";

    bool use_dns = false;
    bool use_mdns = false;

    uint16_t http_tcp_port = 80;

    // Maximum number of connection attempts for configured acces point in station mode
    int max_reconnections = 4;
    uint32_t reconnection_timeout_ms = 3000;
    uint32_t dns_ttl = 3000;
};

#endif
