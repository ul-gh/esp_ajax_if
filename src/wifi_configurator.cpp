/* WiFi configuration using ESP-IDF NVS subsystem for persistent config.
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
// In FUTURE when arduino-esp32 supports IDF v4.x
//#include "nvs_handle.hpp"

#include <WiFi.h>
#include <ESPmDNS.h>

// Local debug level
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "wifi_configurator.cpp";

#include "wifi_configurator.hpp"


WiFiConfigurator::WiFiConfigurator(AsyncWebServer *http_backend,
                                   DNSServer *dns_server,
                                   NetworkConfig &conf)
    // HTTP AJAX API server instance was created before
    : http_backend{http_backend}
    , dns_server{dns_server}
    , conf{conf}
{
    assert(http_backend);
    // Initialize NVS
    auto err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    //std::shared_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("eal_storage", NVS_READWRITE, &result);
    err |= nvs_open("eal_storage", NVS_READWRITE, &_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS. Abort.");
        abort();
    }
    _get_nvs_state();
}

WiFiConfigurator::~WiFiConfigurator() {
    http_backend->removeHandler(_http_request_handler);
    nvs_close(_nvs_handle);
}

// This is supposed to block until WiFi is up and running...
void WiFiConfigurator::begin() {
    ESP_LOGI(TAG, "This is restart no.: %d\n", _restart_counter);
    if (_restart_counter > 3) {
        ESP_LOGI(TAG, "Max. restarts reached. Could not connect to WiFi in station mode.");
        conf.ap_mode_active = true;
        _reconfigure_current_mode();
    }
    ESP_LOGI(TAG, "Try setting up WiFi with saved configuration...");
    auto stored_mode_is_ap = WiFi.getMode() == WIFI_AP;
    if (stored_mode_is_ap != conf.ap_mode_active) {
        _reconfigure_current_mode();
    } else {
        if (conf.ap_mode_active) {
            _reconnect_ap_mode();
        } else {
            _reconnect_station_mode();
        }
    }
    // We should now be connected..
    auto err = nvs_set_u8(_nvs_handle, "restart_counter", 0);
    _register_http_api();

    if (conf.use_dns) {
        _setup_dns_server();
    }
}

void WiFiConfigurator::_counting_device_restart() {
    ESP_LOGW(TAG, "Max. connection retries exhausted. Restart...");
    _restart_counter++;
    auto err = nvs_set_u8(_nvs_handle, "restart_counter", _restart_counter);
    err |= nvs_commit(_nvs_handle);
    ESP_ERROR_CHECK( err );
    fflush(stdout);
    ESP.restart();
}

void WiFiConfigurator::_get_nvs_state() {
    auto err = ESP_OK;
    // _restart counter defaults to 0
    err |= nvs_get_u8(_nvs_handle, "restart_counter", &_restart_counter);

    // Fallback value from config header
    auto u8_flag = static_cast<u8_t>(conf.ap_mode_active);
    // Persistent activation of access point mode if set to true
    err |= nvs_get_u8(_nvs_handle, "ap_mode", &u8_flag);
    conf.ap_mode_active = static_cast<bool>(u8_flag);
    // Auto-configure ip4 address in station mode when set to true
    err |= nvs_get_u8(_nvs_handle, "sta_dhcp", &u8_flag);
    conf.sta_mode_use_dhcp = static_cast<bool>(u8_flag);
    // IPv4 address for both access point or station mode
    auto u32_val = static_cast<uint32_t>(conf.ip4_addr);
    err |= nvs_get_u32(_nvs_handle, "ip4_addr", &u32_val);
    conf.ip4_addr = IPAddress{u32_val};
    // IPv4 gateway
    u32_val = static_cast<uint32_t>(conf.ip4_gw);
    err |= nvs_get_u32(_nvs_handle, "ip4_gw", &u32_val);
    conf.ip4_gw = IPAddress{u32_val};
    // IPv4 netmask 
    u32_val = static_cast<uint32_t>(conf.ip4_mask);
    err |= nvs_get_u32(_nvs_handle, "ip4_mask", &u32_val);
    conf.ip4_mask = IPAddress{u32_val};
    // SSID
    auto str_len = conf.ssid_maxlen;
    err |= nvs_get_str(_nvs_handle, "ssid", conf.ssid, &str_len);
    // WPA-PSK
    str_len = conf.psk_maxlen;
    err |= nvs_get_str(_nvs_handle, "psk", conf.psk, &str_len);
    // Hostname
    str_len = conf.hostname_maxlen;
    err |= nvs_get_str(_nvs_handle, "hostname", conf.hostname, &str_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read persistent state from NVS!");
    }
}

void WiFiConfigurator::_set_nvs_state() {
    auto err = ESP_OK;
    auto u8_flag = static_cast<u8_t>(conf.ap_mode_active);
    err |= nvs_set_u8(_nvs_handle, "ap_mode", u8_flag);
    // Auto-configure ip4 address in station mode when set to true
    u8_flag = static_cast<u8_t>(conf.sta_mode_use_dhcp);
    err |= nvs_set_u8(_nvs_handle, "sta_dhcp", u8_flag);
    // IPv4 address for both access point or station mode
    auto u32_val = static_cast<uint32_t>(conf.ip4_addr);
    err |= nvs_set_u32(_nvs_handle, "ip4_addr", u32_val);
    // IPv4 gateway 
    u32_val = static_cast<uint32_t>(conf.ip4_gw);
    err |= nvs_set_u32(_nvs_handle, "ip4_gw", u32_val);
    // IPv4 netmask 
    u32_val = static_cast<uint32_t>(conf.ip4_mask);
    err |= nvs_set_u32(_nvs_handle, "ip4_mask", u32_val);
    // SSID
    err |= nvs_set_str(_nvs_handle, "ssid", conf.ssid);
    // WPA-PSK
    err |= nvs_set_str(_nvs_handle, "psk", conf.psk);
    // Hostname
    err |= nvs_set_str(_nvs_handle, "hostname", conf.hostname);
    // Commit all changes to NVS
    err |= nvs_commit(_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write persistent state to NVS!");
    }
}

void WiFiConfigurator::_reconnect_ap_mode() {
    for (auto retries = 0; retries < conf.max_reconnections; retries++) {
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            break;
        }
        if (retries >= conf.max_reconnections) {
            _counting_device_restart();
        }
        ESP_LOGW(TAG, "Timeout. Retrying..");
        WiFi.begin();
        delay(conf.reconnection_timeout_ms);
    }
    // We are now connected.. Restoring configuration from stored connection..
    ESP_LOGI(TAG, "Set up access point with SSID: %s  and IP address: %s",
             conf.ssid, conf.ip4_addr.toString().c_str());
}

void WiFiConfigurator::_reconnect_station_mode() {
    for (auto retries = 0; retries < conf.max_reconnections; retries++) {
        if (WiFi.softAP(conf.ssid, conf.psk) && WiFi.softAPIP() == conf.ip4_addr) {
            break;
        }
        if (retries >= conf.max_reconnections) {
            _counting_device_restart();
        }
        ESP_LOGW(TAG, "Timeout. Retrying..");
        WiFi.begin();
        delay(conf.reconnection_timeout_ms);
    }
    // We are now connected.. Restoring configuration from stored connection..
    // When using DHCP, the configuration can have changed.
    // The changes are added to the instance properties but not made persistent.
    conf.ip4_addr = WiFi.localIP();
    conf.ip4_gw = WiFi.gatewayIP();
    conf.ip4_mask = WiFi.subnetMask();
    strncpy(conf.hostname, WiFi.getHostname(), conf.hostname_maxlen);
    strncpy(conf.ssid, WiFi.SSID().c_str(), conf.ssid_maxlen);
    ESP_LOGI(TAG, "Connected as host %s with IP address: %s",
             conf.hostname, conf.ip4_addr.toString().c_str());
}

void WiFiConfigurator::_reconfigure_current_mode() {
    _set_nvs_state();
    if (conf.ap_mode_active) {
        _configure_ap_mode();
        _reconnect_ap_mode();
    } else {
        _configure_station_mode();
        _reconnect_station_mode();
    }
}

void WiFiConfigurator::_configure_station_mode() {
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(conf.hostname);
    if (!conf.sta_mode_use_dhcp) {
        WiFi.config(conf.ip4_addr, conf.ip4_gw, conf.ip4_mask);
    }
    // Connect to Wi-Fi network with SSID and password
    ESP_LOGI(TAG, "(Re-)Connecting to SSID: %s", conf.ssid);
    WiFi.begin(conf.ssid, conf.psk);
}

void WiFiConfigurator::_configure_ap_mode() {
    ESP_LOGI(TAG, "Setting soft-AP mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(conf.ip4_addr, conf.ip4_gw, conf.ip4_mask);
    WiFi.softAPsetHostname(conf.hostname);
    if (WiFi.softAP(conf.ssid, conf.psk)) {
        ESP_LOGI(TAG, "Soft-AP IP address: %s", WiFi.softAPIP().toString().c_str());
    } else {
        ESP_LOGE(TAG, "Setting up Access Point failed! Aborting...");
        abort();
    }
}

/* Register WiFi configuration HTTP POST API callback into the server
 */
void WiFiConfigurator::_register_http_api() {
    constexpr size_t json_obj_size = JSON_OBJECT_SIZE(5);
    constexpr size_t keys_size = sizeof(
        "hostname""ip4_addr""ip4_gw""ip4_mask""ssid""psk""ap_mode""sta_dhcp"
        );
    // WPA2 SSID and PSK
    constexpr size_t data_strings_size = NetworkConfig::hostname_maxlen
                                         + NetworkConfig::ssid_maxlen
                                         + NetworkConfig::psk_maxlen;
    constexpr size_t json_buf_size = 50 + json_obj_size + keys_size + data_strings_size;
    // "/configure_wifi"
    _http_request_handler = new AsyncCallbackJsonWebHandler(
        "/configure_wifi",
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            auto json_obj = json.as<JsonObject>();
            _on_request_do_configuration(json_obj);
            request->send(200, "text/plain", "OK");
        },
        json_buf_size
    );
    http_backend->addHandler(_http_request_handler);
}


/* Configure WiFi on API request
 */
void WiFiConfigurator::_on_request_do_configuration(JsonObject &json_obj) {
    conf.ip4_addr = IPAddress{uint32_t{json_obj["ip4_addr"]}};
    conf.ip4_gw = IPAddress{uint32_t{json_obj["ip4_gw"]}};
    conf.ip4_mask = IPAddress{uint32_t{json_obj["ip4_mask"]}};
    strncpy(conf.hostname, json_obj["hostname"].as<char*>(), conf.hostname_maxlen);
    strncpy(conf.ssid, json_obj["ssid"].as<char*>(), conf.ssid_maxlen);
    strncpy(conf.psk, json_obj["psk"].as<char*>(), conf.psk_maxlen);
    conf.ap_mode_active = bool{json_obj["ap_mode"]};
    conf.sta_mode_use_dhcp = bool{json_obj["sta_dhcp"]};
    _reconfigure_current_mode();
}


/* Configures the DNSServer instance
 */
void WiFiConfigurator::_setup_dns_server() {
    if (!dns_server) {
        return;
    }
  // DNS caching TTL associated  with the domain name.
  dns_server->setTTL(conf.dns_ttl);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is AsyncDNSReplyCode::NonExistentDomain
  //dns_server.setErrorReplyCode(AsyncDNSReplyCode::ServerFailure);
  auto dns_domain = String(conf.hostname) + conf.dns_tld;
  dns_server->start(53, dns_domain, conf.ip4_addr);
}
