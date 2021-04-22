/* WiFi configuration using ESP-IDF NVS subsystem for persistent config.
 *
 * Configuration is triggered by HTTP API server instance,
 * see ApiServer class (api_server.cpp)
 *
 * There is NO flash encryption!
 * There is NO security implemented other than on the network link-level!
 * 
 * License: GPL v.3 
 * U. Lukas 2021-04-21
 */
#include "esp_system.h"
#include "nvs_flash.h"
// In FUTURE when arduino-esp32 supports IDF v4.x
//#include "nvs_handle.hpp"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

// Local debug level
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "wifi_configurator.cpp";

#include "wifi_configurator.hpp"


WiFiConfigurator::WiFiConfigurator(NetworkConfig &conf,
                                   AsyncWebServer *http_backend,
                                   DNSServer *dns_server)
    // HTTP AJAX API server instance was created before
    : conf{conf}
    , http_backend{http_backend}
    , dns_server{dns_server}
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
    _restore_state_from_nvs();
}

WiFiConfigurator::~WiFiConfigurator() {
    http_backend->removeHandler(_http_request_handler);
    nvs_close(_nvs_handle);
}

// This is supposed to block or reboot until WiFi is up and running...
void WiFiConfigurator::begin() {
    // The number of restarts was obtained from NVS on constructor call before.
    // We suppose that AP mode with fallback defaults can not fail and so
    // there is no protection against infinite reboots in case of wrong
    // default values for AP mode
    ESP_LOGI(TAG, "This is restart no.: %d", _restart_counter);
    if (_restart_counter > conf.max_reboots) {
        ESP_LOGI(TAG, "Max. restarts reached. Could not connect to WiFi in station mode.");
        // Restore default configuration
        conf = NetworkConfig{};
        conf.ap_mode_active = true;
        _save_state_to_nvs();
    }
    ESP_LOGI(TAG, "Try setting up WiFi with saved configuration...");
    if ((WiFi.getMode() == WIFI_AP) != conf.ap_mode_active) {
        _reconfigure_reconnect_network_interface();
    } else {
        if (conf.ap_mode_active) {
            _reconnect_ap_mode();
        } else {
            _reconnect_station_mode();
        }
    }
    // We should now be connected and we can reset the restart counter..
    auto err = nvs_set_u8(_nvs_handle, "restart_counter", 0);
    err |= nvs_commit(_nvs_handle);
    ESP_ERROR_CHECK( err );

    _register_http_api();

    if (conf.dns_active) {
        _setup_dns_server();
    }
    if (conf.mdns_active) {
        _setup_mdns_server();
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

void WiFiConfigurator::_restore_state_from_nvs() {
    auto err = ESP_OK;
    // _restart counter defaults to 0
    err |= nvs_get_u8(_nvs_handle, "restart_counter", &_restart_counter);
    // Persistent activation of access point mode if set to true
    auto u8_flag = static_cast<uint8_t>(conf.ap_mode_active);
    err |= nvs_get_u8(_nvs_handle, "ap_mode_active", &u8_flag);
    conf.ap_mode_active = static_cast<bool>(u8_flag);
    // Auto-configure ip4 address in station mode when set to true
    u8_flag = static_cast<uint8_t>(conf.sta_use_dhcp);
    err |= nvs_get_u8(_nvs_handle, "sta_dhcp", &u8_flag);
    conf.sta_use_dhcp = static_cast<bool>(u8_flag);
    // Activate DNS when set to true
    u8_flag = static_cast<uint8_t>(conf.dns_active);
    err |= nvs_get_u8(_nvs_handle, "dns_active", &u8_flag);
    conf.dns_active = static_cast<bool>(u8_flag);
    // Activate MDNS when set to true
    u8_flag = static_cast<uint8_t>(conf.mdns_active);
    err |= nvs_get_u8(_nvs_handle, "mdns_active", &u8_flag);
    conf.mdns_active = static_cast<bool>(u8_flag);
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

void WiFiConfigurator::_save_state_to_nvs() {
    auto err = ESP_OK;
    auto u8_flag = static_cast<uint8_t>(conf.ap_mode_active);
    err |= nvs_set_u8(_nvs_handle, "ap_mode_active", u8_flag);
    // Auto-configure ip4 address in station mode when set to true
    u8_flag = static_cast<uint8_t>(conf.sta_use_dhcp);
    err |= nvs_set_u8(_nvs_handle, "sta_dhcp", u8_flag);
    // Activate DNS when set to true
    u8_flag = static_cast<uint8_t>(conf.dns_active);
    err |= nvs_set_u8(_nvs_handle, "dns_active", u8_flag);
    // Activate MDNS when set to true
    u8_flag = static_cast<uint8_t>(conf.mdns_active);
    err |= nvs_set_u8(_nvs_handle, "mdns_active", u8_flag);
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
    ESP_LOGI(TAG, "Reconnecting AP mode..");
    for (auto retries = 0; retries < conf.max_reconnections; retries++) {
        // Try to re-establish an access point using stored IP configuration
        if (WiFi.softAP(conf.ssid, conf.psk) && WiFi.softAPIP() == conf.ip4_addr) {
            break;
        }
        if (retries >= conf.max_reconnections) {
            _configure_ap_mode();
            _counting_device_restart();
        }
        ESP_LOGW(TAG, "Timeout. Retrying..");
        _configure_ap_mode();
        delay(conf.reconnection_timeout_ms);
    }
    // We are now running an access point (hopefully)..
    ESP_LOGI(TAG, "Set up access point with SSID: %s  and IP address: %s",
             conf.ssid, conf.ip4_addr.toString().c_str());
}

void WiFiConfigurator::_reconnect_station_mode() {
    ESP_LOGI(TAG, "Reconnecting station mode..");
    for (auto retries = 0; retries < conf.max_reconnections; retries++) {
        // Try reconnecting with stored config. If this retruns WL_CONNECTED, we are done..
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
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
            return;
        }
        ESP_LOGW(TAG, "Timeout. Retrying..");
        WiFi.begin();
        delay(conf.reconnection_timeout_ms);
    }
    // Maximum number of reconnections exceeded. Reconfigure and restart device..
    _configure_station_mode();
    _counting_device_restart();
}

// Looks up if this is AP or station mode and calls _configure and _reconnect
// for the respective mode
void WiFiConfigurator::_reconfigure_reconnect_network_interface() {
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
    if (!conf.sta_use_dhcp) {
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
        ESP_LOGE(TAG, "Setting up Access Point failed!");
    }
}

// Register WiFi configuration HTTP POST API callback into the server
void WiFiConfigurator::_register_http_api() {
    constexpr size_t json_obj_size = JSON_OBJECT_SIZE(7);
    constexpr size_t keys_size = sizeof(
        "hostname""ip4_addr""ip4_gw""ip4_mask""ssid""psk"
        "ap_mode_active""sta_use_dhcp""dns_active""mdns_active"
        );
    // WPA2 SSID and PSK
    constexpr size_t data_strings_size = NetworkConfig::hostname_maxlen
                                         + NetworkConfig::ssid_maxlen
                                         + NetworkConfig::psk_maxlen;
    constexpr size_t json_buf_size = 50 + json_obj_size + keys_size + data_strings_size;
    // Register "/get_wifi_config" handler
    http_backend->on(
        conf.get_wifi_config_endpoint,
        HTTP_GET,
        [this](AsyncWebServerRequest *request){_send_config_response(request);}
    );
    // Register "/set_wifi_config" handler
    _http_request_handler = new AsyncCallbackJsonWebHandler(
        conf.set_wifi_config_endpoint,
        [this](AsyncWebServerRequest *request, JsonVariant &jv) {
            auto json_obj = jv.as<JsonObject>();
            _on_request_do_configuration(json_obj);
            _send_config_response(request);
        },
        json_buf_size
    );
    http_backend->addHandler(_http_request_handler);
}


// Configure WiFi on API request
void WiFiConfigurator::_on_request_do_configuration(JsonObject &json_obj) {
    auto ip = IPAddress{};
    auto str = json_obj["ip4_addr"].as<char*>();
    if (str && ip.fromString(str)) {conf.ip4_addr = ip;}
    str = json_obj["ip4_gw"].as<char*>();
    if (str && ip.fromString(str)) {conf.ip4_gw = ip;}
    str = json_obj["ip4_mask"].as<char*>();
    if (str && ip.fromString(str)) {conf.ip4_mask = ip;}
    str = json_obj["hostname"].as<char*>();
    if (str && strlen(str) < sizeof(conf.hostname)) {strcpy(conf.hostname, str);}
    str = json_obj["ssid"].as<char*>();
    if (str && strlen(str) < sizeof(conf.ssid)) {strcpy(conf.ssid, str);}
    str = json_obj["psk"].as<char*>();
    if (str && strlen(str) < sizeof(conf.psk)) {strcpy(conf.psk, str);}
    auto jv = json_obj["ap_mode_active"];
    if (!jv.isNull() && jv.is<bool>()) {conf.ap_mode_active = jv.as<bool>();}
    jv = json_obj["sta_use_dhcp"];
    if (!jv.isNull() && jv.is<bool>()) {conf.sta_use_dhcp = jv.as<bool>();}
    jv = json_obj["dns_active"];
    if (!jv.isNull() && jv.is<bool>()) {conf.dns_active = jv.as<bool>();}
    jv = json_obj["mdns_active"];
    if (!jv.isNull() && jv.is<bool>()) {conf.mdns_active = jv.as<bool>();}
    _reconfigure_reconnect_network_interface();
}


// On request, send configuration JSON encoded as HTTP body
void WiFiConfigurator::_send_config_response(AsyncWebServerRequest *request) {
    auto response = new AsyncJsonResponse();
    auto json_variant = response->getRoot();
    // Const cast necessary as this Arduino JSON version does not make a copy for const char* rvalue IIRC
    json_variant["ip4_addr"] = const_cast<char*>(conf.ip4_addr.toString().c_str());
    json_variant["ip4_gw"] = const_cast<char*>(conf.ip4_gw.toString().c_str());
    json_variant["ip4_mask"] = const_cast<char*>(conf.ip4_mask.toString().c_str());
    json_variant["hostname"] = conf.hostname;
    json_variant["ssid"] = conf.ssid;
    // Nope.... PSK is not submitted back to the client
    // json_doc["psk"] = conf.psk;
    json_variant["ap_mode_active"] = conf.ap_mode_active;
    json_variant["sta_use_dhcp"] = conf.sta_use_dhcp;
    json_variant["dns_active"] = conf.dns_active;
    json_variant["mdns_active"] = conf.mdns_active;
    response->setLength();
    request->send(response);
}

// Configure a DNSServer instance
void WiFiConfigurator::_setup_dns_server() {
    if (!dns_server) {return;}
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

// Optionally, configure MDNS service
void WiFiConfigurator::_setup_mdns_server() {
    MDNS.begin(conf.hostname);
    MDNS.addService("http", "tcp", conf.http_tcp_port);
}
