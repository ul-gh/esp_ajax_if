#ifndef NETWORK_CONFIG_HPP__
#define NETWORK_CONFIG_HPP__
#include "IPAddress.h"
#include "network_config.hpp"

struct NetworkConfig {
    const IPAddress ap_ip{192,168,4,1};
    const IPAddress ap_gateway{192,168,4,1};
    const IPAddress ap_subnet{255,255,255,0};

    static constexpr const char* ap_ssid = "isocal";
    static constexpr const char* ap_psk = "123FOO456";

    static constexpr const char* sta_ssid = "K8M";
    static constexpr const char* sta_psk = "zy807Mk9gy3_hkRt";

    static constexpr const char* host_name = "isocal";
    static constexpr const char* dns_domain = "isocal.lan";

    static constexpr bool use_mdns = false;

    static constexpr uint16_t http_tcp_port = 80;

    // WiFi Manager can launch the ESP in access point mode and serve a
    // captive portal page for WiFi configuration.
    // This mostly makes sense for use as a WiFi client.
    static constexpr bool use_wifi_manager = false;
};

#endif
