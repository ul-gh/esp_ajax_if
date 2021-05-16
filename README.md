\mainpage
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


## HTML Documentation:
[ESP-LiveControl](https://ul-gh.github.io/esp_ajax_if/)

## License
[GPL v3.0](./LICENSE)

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


## C++ Application Data Model
* [AppState](struct_app_state.html)

## C++ Application Controller Implementing C++ and HTTP GET API
This features the main control functions for PWM frequency, duty cycle etc.

Also, periodic state feedback for all hardware functions is sent to the
HTTP remote application using Server-Sent Events from a FreeRTOS timer task.

Some auxiliary functions like GPIO and temperature readouts is outsourced
to class AuxHwDrv.
* [AppController](class_app_controller.html)

## C++ HTTP and SSE Event Server
* [APIServer](class_a_p_i_server.html)

## Auxiliary Hardware Control and Utility Classes
* [AuxHwDrv](class_aux_hw_drv.html)
* [SensorKTY81_1xx](class_sensor_k_t_y81__1xx.html)
* [ESP32ADCChannel](class_e_s_p32_a_d_c_channel.html)
* [U16MovingAverageUInt16](class_moving_average_u_int16.html)
* [EquidistantPWLUInt16](class_equidistant_p_w_l_u_int16.html)
* [MultiTimer](class_multi_timer.html)

## Low-Level-Driver (ESP-IDF Compatible)
* See file:<br>
[components/esp32_ps_pwm/ps_pwm.h](ps__pwm_8h.html)

## Application Configuration
* [main/config/app_config.hpp](app__config_8hpp.html)


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

* User settable minimum output frequency in kHz:<br>
/cmd?set_frequency_min=50

* User settable maximum output frequency in kHz:<br>
/cmd?set_frequency_max=300

* Set frequency in kHz:<br>
/cmd?set_frequency=500

* Set bridge output resulting duty cycle in %:<br>
/cmd?set_duty=45.3

* set lag bridge leg dead time in nanoseconds:<br>
/cmd?set_lag_dt=300

* set lead bridge leg dead time in nanoseconds:<br>
/cmd?set_lead_dt=300

* Activate|deactivate the output:<br>
/cmd?set_power_pwm_active=true|false

* Set the length of the one-shot power output pulse in seconds:<br>
/cmd?set_oneshot_len=40.5

* Trigger a one-shot output power pulse of configurable length:<br>
/cmd?trigger_oneshot

* Clear error shutdown condition:<br>
/cmd?clear_shutdown

* Set hardware overcurrent detection circuit threshold current:<br>
/cmd?set_current_limit=35

* Activate|deactivate the power output load switches:<br>
/cmd?set_relay_ref_active=true|false<br>
/cmd?set_relay_dut_active=true|false

* Manual override of the heatsink fan, stays "on" when true:<br>
/cmd?set_fan_override=true|false

* Save all runtime settings to SPI flash:<br>
/cmd?save_settings

### Server sends periodic application status update via Server-Sent Events:
* SSE event source endpoint:<br>
/events

* Event source telegram content is JSON with content:
```
{
    // Setpoint limits from PWM hardware constraints.
    // Scaled to kHz, ns and % respectively...
    "frequency_min_hw": Frequency in kHz
    "frequency_max_hw": Frequency in kHz
    "dt_sum_max_hw": Dead-Time in nanoseconds
    // Setpoint limit custom user settings. See GET API above.
    "frequency_min": Frequency in kHz
    "frequency_max": Frequency in kHz
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
    "fan_override": Boolean

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
    // Length of the power output one-shot timer pulse
    json_doc["oneshot_len"] = oneshot_power_pulse_length_ms/1e3;
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
