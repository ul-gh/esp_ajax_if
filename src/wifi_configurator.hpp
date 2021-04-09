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

#include "app_config.hpp"


/** @brief WiFi configuration using ESP-IDF NVS subsystem for persistent config.
 *
 * Configuration is triggered by HTTP API server instance,
 * see ApiServer class (api_server.cpp)
 *
 * There is NO flash encryption!
 * There is NO security implemented other than on the network link-level!
 */
class WiFiConfigurator
{
public:
    WiFiConfigurator(AsyncWebServer *http_backend,
                     DNSServer *dns_server,
                     NetworkConfig &conf);

    ~WiFiConfigurator();

    // This is supposed to block until WiFi is up and running...
    void begin();

private:
    AsyncWebServer *http_backend;
    DNSServer *dns_server;
    NetworkConfig &conf;

    nvs_handle_t _nvs_handle;
    uint8_t _restart_counter = 0;

    // ESPAsyncWebServer HTTP request handler for WiFi configuration API endpoint 
    AsyncCallbackJsonWebHandler *_http_request_handler = nullptr;

    void _counting_device_restart();

    void _get_nvs_state();
    void _set_nvs_state();

    void _reconnect_ap_mode();
    void _reconnect_station_mode();

    void _reconfigure_current_mode();

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