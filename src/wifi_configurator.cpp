/* Wifi configuration using ESP-IDF NVS subsystem for persistent config.
 *
 * Configuration is triggered by HTTP API server instance,
 * see ApiServer class (api_server.cpp)
 *
 * There is NO flash encryption!
 * There is NO security implemented other than the network link-level!
 * 
 * License: GPL v.3 
 * U. Lukas 2021-04-08
 */
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

// Local debug level
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "wifi_configurator.cpp";

#include "wifi_configurator.hpp"
#include "network_config.hpp"


WifiConfigurator::WifiConfigurator(AsyncWebServer *http_backend,
                                   DNSServer *dns_server,
                                   struct NetworkConfig &net_conf)
    // HTTP AJAX API server instance was created before
    : http_backend{http_backend}
    , dns_server{dns_server}
    , net_conf{net_conf}
{
    assert(http_backend);
    std::shared_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &result);
}

WifiConfigurator::~WifiConfigurator() {
}

// This is supposed to block until WiFi is up and running...
void WifiConfigurator::begin() {
    _register_http_api();
}

void WifiConfigurator::_on_boot_setup_wifi(uint8_t max_retries) {
    ESP_LOGI(TAG, "Try WiFi connection with saved credentials...");
    while (WiFi.waitForConnectResult() != WL_CONNECTED && max_retries > 0) {
        ESP_LOGW(TAG, "Timeout. Retrying..");
        WiFi.begin();
        delay(3000);
        --max_retries;
    }

    if(max_retries == 0) {
        ESP_LOGW(TAG, "Max. retries exhausted. Restarting...");
        ESP.restart();
    }

    // We are now connected..
    ESP_LOGI(TAG, "Connected with IP address: %s", WiFi.localIP().toString().c_str());
}

void WifiConfigurator::_setup_wifi_station() {
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    // Connect to Wi-Fi network with SSID and password
    ESP_LOGI(TAG, "(Re-)Connecting to SSID: %s", net_conf.sta_ssid);
    WiFi.setHostname(net_conf.host_name);
    WiFi.begin(net_conf.sta_ssid, net_conf.sta_psk);
}

void WifiConfigurator::_setup_wifi_hostap() {
}


/* Register all application HTTP GET API callbacks into the HTPP server
 */
void WifiConfigurator::_register_http_api() {
    // "/configure_wifi"
    http_backend->on("configure_wifi", HTTP_POST, [this](AsyncWebServerRequest *request){
        _do_configuration(request);
    });
}


/* Configures the DNSServer instance
 */
void WifiConfigurator::_setup_dns_server() {
    if (!dns_server) {
        return;
    }
  // DNS caching TTL associated  with the domain name. Default is 60 seconds.
  dns_server->setTTL(300);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is AsyncDNSReplyCode::NonExistentDomain
  //dns_server.setErrorReplyCode(AsyncDNSReplyCode::ServerFailure);
  dns_server->start(53, net_conf.dns_domain, net_conf.ap_ip);
}

/* Configures on API request
 */
void WifiConfigurator::_do_configuration(AsyncWebServerRequest *request) {

}