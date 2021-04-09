/** @file wifi_configurator.hpp
 * 
 * License: GPL v.3 
 * U. Lukas 2021-04-08
 */
#ifndef WIFI_CONFIGURATOR_HPP__
#define WIFI_CONFIGURATOR_HPP__

#include "nvs.h"

#include <DNSServer.h>

#include <ESPAsyncWebServer.h>
#include "ArduinoJson.h"
#include "AsyncJson.h"

#include "network_config.hpp"

static constexpr size_t ssid_maxlen = 32 + 1;
static constexpr size_t psk_maxlen = 63 + 1;
static constexpr size_t hostname_maxlen = 32 + 1;

/** @brief WiFi configuration using ESP-IDF NVS subsystem for persistent config.
 *
 * Configuration is triggered by HTTP API server instance,
 * see ApiServer class (api_server.cpp)
 *
 * There is NO flash encryption!
 * There is NO security implemented other than the network link-level!
 */
class WiFiConfigurator
{
public:
    bool ap_mode_active;
    char hostname[hostname_maxlen];
    IPAddress ip4_addr;
    IPAddress ip4_gw;
    IPAddress ip4_mask;
    char ssid[ssid_maxlen];
    char psk[psk_maxlen];

    WiFiConfigurator(AsyncWebServer *http_backend,
                     DNSServer *dns_server,
                     NetworkConfig &net_conf);

    ~WiFiConfigurator();

    // This is supposed to block until WiFi is up and running...
    void begin();

private:
    AsyncWebServer *http_backend;
    DNSServer *dns_server;
    NetworkConfig &net_conf;
    nvs_handle_t _nvs_handle;
    uint8_t _restart_counter = 0;
    // ESPAsyncWebServer HTTP request handler for WiFi configuration API endpoint 
    AsyncCallbackJsonWebHandler *_http_request_handler = nullptr;

    void _get_persistent_state();
    void _set_persistent_state();

    void _reconnect_station_mode();
    void _run_wifi_ap_mode();

    void _configure_station_mode();
    void _configure_ap_mode();

    void _on_request_do_configuration(JsonObject &json_obj);

    /* Register all application HTTP GET API callbacks into the HTPP server
    */
    void _register_http_api();

    /* Configures the DNSServer instance
    */
    void _setup_dns_server();
};

#endif /* WIFI_CONFIGURATOR_HPP__ */