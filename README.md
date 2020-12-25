# ESP-AJAX-Lab
HTTP server, AJAX API backend, web application and Soc hardware drivers
for WiFi remote control of the MCPWM hardware modules on the
Espressif ESP32 SoC

Default configuration set up for generating a Phase-Shift-PWM waveform
between two pairs of hardware pins. This also features auxiliary
measurement and control functions:

* Calibrated temperature sensor readout for KTY81-121 type silicon
  temperature sensors using the ESP32 ADC in its high-linearity region
* PWM reference signal generation for hardware overcurrent detector
* External GPIO output control for relays, fan, enable and error-reset
* TBD: Delta-Sigma conversion control

The HTML web application interface features a responsive CSS grid layout and
requires a web browser with at least JavaScript ES 7 support.

The toolchain compiler must support at least std=c++17.

## HTML Documentation
[ESP-AJAX-LAB](https://ul-gh.github.io/esp_ajax_if/)

## License
[GPL v3.0](./LICENSE)
