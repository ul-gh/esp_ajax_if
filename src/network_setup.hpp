#ifndef NETWORK_SETUP_HPP__
#define NETWORK_SETUP_HPP__

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

// At initilisation phase, wifi stack debug output suppresses serial console
// output for a certaion time. Setting a small delay here adds to bootup time
// but enables startup messages from application to appear
#define WIFI_INIT_DELAY 200

enum WIFI_EVENT{NO_CHANGE, NEW_CONNECTION, DISCONNECTION};

void setup_wifi_hostap(const struct NetworkConfig &net_conf);
void setup_wifi_station(const struct NetworkConfig &net_conf);

enum WIFI_EVENT wifi_handle_state_change();

/* Configures the DNSServer instance
 */
void setup_dns_server(DNSServer &dns_server, const struct NetworkConfig &net_conf);

/* WiFi Manager can launch the ESP in access point mode and serve a
 * captive portal page for WiFi configuration.
 *  This mostly makes sense for use as a WiFi client.
 */
void run_wifi_manager(DNSServer *dns_server, AsyncWebServer *http_backend);

#endif