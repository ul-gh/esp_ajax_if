/** @file wifi_configurator.hpp
 * 
 * License: GPL v.3 
 * U. Lukas 2021-04-08
 */
#ifndef WIFI_CONFIGURATOR_HPP__
#define WIFI_CONFIGURATOR_HPP__

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

// Local debug level
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "wifi_configurator.cpp";

#include "wifi_configurator.hpp"
#include "network_config.hpp"


/** @brief Wifi configuration using ESP-IDF NVS subsystem for persistent config.
 *
 * Configuration is triggered by HTTP API server instance,
 * see ApiServer class (api_server.cpp)
 *
 * There is NO flash encryption!
 * There is NO security implemented other than the network link-level!
 */
class WifiConfigurator
{
public:
    WifiConfigurator(AsyncWebServer *http_backend,
                     DNSServer *dns_server,
                     struct NetworkConfig &net_conf);

    ~WifiConfigurator() {
    }

    // This is supposed to block until WiFi is up and running...
    void begin();

private:
    AsyncWebServer *http_backend;
    DNSServer *dns_server;
    const struct NetworkConfig &net_conf;

    void _on_boot_setup_wifi(uint8_t max_retries);

    void _setup_wifi_station();

    void _setup_wifi_hostap();

    void _do_configuration(AsyncWebServerRequest *request);

    /* Register all application HTTP GET API callbacks into the HTPP server
    */
    void _register_http_api();

    /* Configures the DNSServer instance
    */
    void _setup_dns_server();
};

#endif /* WIFI_CONFIGURATOR_HPP__ */