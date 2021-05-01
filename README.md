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

The toolchain compiler must support at least std=c++17.

## HTML Documentation: [ESP-AJAX-LAB](https://ul-gh.github.io/esp_ajax_if/)

## License
[GPL v3.0](./LICENSE)
