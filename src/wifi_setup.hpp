#ifndef __WIFI_SETUP_HPP__
#define __WIFI_SETUP_HPP__

// At initilisation phase, wifi stack debug output suppresses serial console
// output for a certaion time. Setting a small delay here adds to bootup time
// but enables startup messages from application to appear
#define WIFI_INIT_DELAY 200

enum WIFI_EVENT{NO_CHANGE, NEW_CONNECTION, DISCONNECTION};

void setup_wifi_hostap();
void setup_wifi_station();

enum WIFI_EVENT wifi_handle_state_change();

#endif