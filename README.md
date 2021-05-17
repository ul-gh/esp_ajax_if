# ESP-LiveControl
HTTP server, AJAX API backend and Vue.js web application implementing
self-contained, zero-install WiFi remote control of hardware modules
attached to the Espressif ESP32 SoC.

Current application is set up for the electronics hardware laboratory,
generating a Phase-Shift-PWM waveform between two pairs of hardware pins
using the full-featured ESP32 MCPWM module.

This also features auxiliary measurement and control functions:

* Calibrated temperature sensor readout for KTY81-121 type silicon
  temperature sensors using the ESP32 ADC in its high-linearity region
* PWM reference signal generation for hardware overcurrent detector
* External GPIO output control for relays, fan, enable and error-reset


## Cloud-Free (On-Board-Hosted) Single-Page Web Application
Stored in on-board flash (SPIFFS/LittleFS), served by an instance of
ESPAsyncWebServer and coupled using a fully-async HTTP GET and SSE push API,
the web application implements a tabbed-view hardware remote-control
user interface designed for either mobile, hardware dashboard or PC.

The web application is designed to serve as a live-feedback,
fairly responsive, hardware remote user interface, running in a
point-to-point fashion on an isolated network.

The web app is built using [Vue.js v3](https://v3.vuejs.org/).

User interface widgets are implemented as vue.js single-file-components.

API interface and remote state is implemented in a state store object
using the vue [composition API](https://v3.vuejs.org/guide/composition-api-introduction.html)
in file: vue_app/src/api/useApiStore.js. 

WiFi/Network configuration is done using a dedicated WiFiConfigurator
application component, designed for to be as-well suited for
permanent access-point mode as for joining an existing network.

WiFi credentials are stored in NVS storage separate from application,
but NVS/flash encryption is OFF.

SSL is (currently) /not/ implemented for the web server or application.


## Server Component and API Documentation (Doxygen):
* [ESP-LiveControl](https://ul-gh.github.io/esp_ajax_if/)


## Screenshots
![Appliation Main Page](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/01_LiveControl.png?raw=true)

![Appliation Settings](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/02_Settings.png?raw=true)

![WiFi Network Configuration](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/03_WiFi_Configurator.png?raw=true)

## License
[GPL v3.0](./LICENSE)
