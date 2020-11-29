#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWiFiManager.h>

// Local debug level
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "network_setup.cpp";

#include "network_setup.hpp"
#include "network_config.hpp"

/* Configure WiFi for Access Point Mode
 */
void setup_wifi_hostap(const struct NetworkConfig &net_conf) {
    WiFi.mode(WIFI_AP);
    //info_print_sv(
    //    "Setting soft-AP configuration ...",
    //    WiFi.softAPConfig(conf_ap_ip, conf_ap_gateway, conf_ap_subnet) ? "Ready" : "Failed!");
    WiFi.softAPsetHostname(net_conf.host_name);
    ESP_LOGI(TAG, "Setting soft-AP mode...");
    if (WiFi.softAP(net_conf.ap_ssid, net_conf.ap_psk)) {
        ESP_LOGI(TAG, "Soft-AP IP address: %s", WiFi.softAPIP().toString().c_str());
    } else {
        ESP_LOGE(TAG, "Setting up Access Point failed! Aborting...");
        abort();
    }
}

/* Configure WiFi for Station Mode
 */
void setup_wifi_station(const struct NetworkConfig &net_conf) {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    // Connect to Wi-Fi network with SSID and password
    ESP_LOGI(TAG, "(Re-)Connecting to SSID: %s", net_conf.sta_ssid);
    WiFi.setHostname(net_conf.host_name);
    WiFi.begin(net_conf.sta_ssid, net_conf.sta_psk);
}

enum WIFI_EVENT wifi_handle_state_change(const struct NetworkConfig &net_conf) {
    static bool was_connected = false;
    bool is_connected = WiFi.status() == WL_CONNECTED;
    if (is_connected && !was_connected) {
        #ifdef WIFI_INIT_DELAY
        delay(WIFI_INIT_DELAY);
        #endif
        ESP_LOGI(TAG, "WiFi connected. IP address: %s", WiFi.localIP().toString().c_str());
        if (net_conf.use_mdns) {
            MDNS.begin(net_conf.host_name);
            MDNS.addService("http", "tcp", net_conf.http_tcp_port);
        }
        was_connected = true;
        return NEW_CONNECTION;
    } else if (!is_connected && was_connected) {
        ESP_LOGW(TAG, "WiFi disconnected!");
        was_connected = false;
        return DISCONNECTION;
    } else {
        return NO_CHANGE;
    }
}

/* Configures the DNSServer instance
 */
void setup_dns_server(DNSServer &dns_server, const struct NetworkConfig &net_conf) {
  // DNS caching TTL associated  with the domain name. Default is 60 seconds.
  dns_server.setTTL(300);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is AsyncDNSReplyCode::NonExistentDomain
  //dns_server.setErrorReplyCode(AsyncDNSReplyCode::ServerFailure);
  dns_server.start(53, net_conf.dns_domain, net_conf.ap_ip);
}

/* WiFi Manager can launch the ESP in access point mode and serve a
 * captive portal page for WiFi configuration.
 *  This mostly makes sense for use as a WiFi client.
 */
void run_wifi_manager(DNSServer *dns_server, AsyncWebServer *http_backend){
    AsyncWiFiManager wifi_manager{http_backend, dns_server};
    // For Debug etc., this always starts the config portal
    //wifi_manager.startConfigPortal("Isocal_Access_Point"); // Debug or special
    // Also for debug etc. causes the config portal to launch with defaults
    //wifi_manager.resetSettings();
    wifi_manager.setConfigPortalTimeout(180);
    // Starts the WiFiManager in modal mode.
    // This blocks when the config portal is running until reboot.
    // When WiFi configuration is stored successfully, this will terminate.
    if(!wifi_manager.autoConnect("Isocal_Access_Point")) {
      Serial.println("Failed to connect and hit timeout");
      delay(1000);
      // Reset and try again
      ESP.restart();
    }
}

/*
void get_set_configuration() {
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  AsyncWiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  AsyncWiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  AsyncWiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server,&dns);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}
*/