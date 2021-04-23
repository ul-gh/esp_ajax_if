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

#include "app_state_model.hpp"


/** @brief Reconnect to or set up a WiFi network connection
 * 
 * This is different from other WiFi manager implementations in that
 * we support configuring either a permanent access-point or station mode.
 * 
 * Configuration is done using a HTTP POST API receiving JSON configuration
 * data under the endpoint configured in struct NetworkConfig (app_config.hpp).
 * 
 * Configuration is submitted in plain text and security is link-level only!
 * 
 * The configuration is stored using the ESP32 NVS subsystem.
 * 
 * There is NO flash memory encryption - please do not re-use the password
 * for any sensitive purpose..
 */
class WiFiConfigurator
{
public:
    WiFiConfigurator(AppState &state,
                     AsyncWebServer *http_backend,
                     DNSServer *dns_server);

    ~WiFiConfigurator();

    /** @brief Reconnect to or set up a WiFi network connection
     * 
     * This function call blocks or reboots the system until a connection
     * is established or the maximum number of reboots is reached.
     */
    void begin();

private:
    AppState &state;
    AsyncWebServer *http_backend;
    DNSServer *dns_server;

    nvs_handle_t _nvs_handle;
    uint8_t _restart_counter = 0;

    // ESPAsyncWebServer HTTP request handler for WiFi configuration API endpoint 
    AsyncCallbackJsonWebHandler *_http_request_handler = nullptr;

    void _counting_device_restart();

    esp_err_t _restore_state_from_nvs(NetworkConfig &conf);
    esp_err_t _save_state_to_nvs(NetworkConfig &conf);

    bool _reconnect_ap_mode(NetworkConfig &conf);
    bool _reconnect_station_mode(NetworkConfig &conf);

    // Looks up if current state says this is AP or station mode and calls
    // _configure and _reconnect for the respective mode
    bool _reconfigure_reconnect_network_interface(NetworkConfig &conf);

    bool _configure_station_mode(NetworkConfig &conf);
    bool _configure_ap_mode(NetworkConfig &conf);

    bool _on_request_do_configuration(JsonObject &json_obj, AsyncWebServerRequest *request);

    // On request, send configuration JSON encoded as HTTP body
    void _send_config_response(NetworkConfig &conf, AsyncWebServerRequest *request);

    // Register all application HTTP GET API callbacks into the HTPP server
    void _register_http_api();

    // Configure a DNSServer instance
    void _setup_dns_server();

    // Optionally, configure MDNS server
    void _setup_mdns_server();
};

#endif /* WIFI_CONFIGURATOR_HPP__ */