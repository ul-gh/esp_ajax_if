\mainpage
# ESP-AJAX-Lab
HTTP server, AJAX API backend, web application and Soc hardware drivers
for WiFi remote control of the MCPWM hardware modules on the
Espressif ESP32 SoC

Default configuration set up for generating a Phase-Shift-PWM waveform
between two pairs of hardware pins. This also features auxiliary
measurement and control functions:

* LUT-calibrated temperature sensor readout for KTY81-121 type silicon
  temperature sensors using the ESP32 ADC in its high-linearity region
* PWM reference signal generation for hardware overcurrent detector
* External GPIO output control for relays, fan, enable and error-reset
* TBD: Delta-Sigma conversion control and filter for insulated measurement
  of power stage current

The HTML web application interface features a responsive CSS grid layout and
requires a newer web browser with at least JavaScript ES 7 support.

## Low-Level-Driver (ESP-IDF compatible)
* See file:<br>
[src/ps_pwm.h](ps__pwm_8h.html)

## C++ API for auxiliary hardware control and temperature sensor readout
* [AuxHwDrv](class_aux_hw_drv.html)
* [AdcTemp](adc__temp_8hpp.html)

## C++ API for PWM generation
* [PsPwmAppHwControl](class_ps_pwm_app_hw_control.html)

## C++ HTTP and SSE event server
* [APIServer](class_a_p_i_server.html)

## HTML+CSS+JS Single-Page Web Application
* [JS in main_app_ajax.js](../../src/data/www/main_app_ajax.js)
* [HTML in main_app.html](../../src/data/www/main_app.html)
* [CSS in main_app.css](../../src/data/www/main_app.css)


## HTTP API Documentation

### Application is AJAX with static HTML5 + CSS 3 + JavaScript ES 7
* Static HTTP content is served from SPI flash file system<br>
(Path in source directory: src/data/www)

### Hardware device is controlled via HTTP GET requests:
* HTTP API endpoint:<br>
/cmd

* Server response to control requests:<br>
HTTP Status 200 OK and plain text content "OK"

### List of HTTP GET requests for application control:

* Set frequency in kHz:<br>
/cmd?set_frequency=500

* Set bridge output resulting duty cycle in %:<br>
/cmd?set_duty=45.3

* set lead bridge leg dead time in nanoseconds:<br>
/cmd?set_lead_dt=300

* set lag bridge leg dead time in nanoseconds:<br>
/cmd?set_lag_dt=300

* Activate|deactivate the output:<br>
/cmd?set_power_pwm_active=true|false

* Clear error shutdown condition:<br>
/cmd?clear_shutdown

* Set hardware overcurrent detection circuit threshold current:<br>
/cmd?set_current_limit=35

* Activate|deactivate the power output load switches:<br>
/cmd?set_relay_ref_active=true|false<br>
/cmd?set_relay_dut_active=true|false

* Activate|deactivate the heatsink fan:<br>
/cmd?set_power_pwm_active=true|false

### Server sends periodic application status update via Server-Sent Events:
* SSE event source endpoint:<br>
/events

* Event source telegram content is JSON with content:
```
{
    // Setpoint limits. Scaled to kHz, ns and % respectively...
    "frequency_min": Frequency in kHz
    "frequency_max": Frequency in kHz
    "dt_sum_max": Dead-Time in nanoseconds
    // Operational setpoints for PSPWM module
    "frequency": Frequency in kHz
    "duty": Duty Cycle in percent
    "lead_dt": Dead-Time in nanoseconds
    "lag_dt": Dead-Time in nanoseconds
    "power_pwm_active": Boolean
    // Settings for auxiliary HW control module
    "current_limit": Current limit in Amperes
    "relay_ref_active": Boolean
    "relay_dut_active": Boolean
    // Temperatures and fan
    "aux_temp": Temperature in °C
    "heatsink_temp": Temperature in °C
    "fan_active": Boolean

    // Clock divider settings
    "base_div": Integer clock divider factor for MCPWM hardware timer
    "timer_div": ditto, second clock divider for MCPWM hardware timer
    // Gate driver supply and disable signals
    "driver_supply_active": Boolean
    "drv_disabled": Boolean
    // True when hardware OC shutdown condition is present
    "hw_oc_fault_present": Boolean
    // Hardware Fault Shutdown Status is latched using this flag
    "hw_oc_fault_occurred": Boolean
}
```

### Example application control from PC side:
* Python example:
```
import requests

url = "http://pwm-generator.local/cmd"

cmd1 = {"set_frequency": 500}
cmd2 = {"set_duty": 45.3}

requests.get(url, cmd1)
requests.get(url, cmd2)
…
```

## License
[GPL v3.0](./LICENSE)
