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


## Server component and C++ API documentation
### C++ Application Data Model
* [AppState](struct_app_state.html)

### C++ Application Controller Implementing C++ and HTTP GET API
* [AppController](class_app_controller.html)<br>
  This features the main control functions for PWM frequency, duty cycle etc.<br>
  Also, periodic state feedback for all hardware functions is sent to the
  HTTP remote application using Server-Sent Events from a FreeRTOS timer task.  
  Some auxiliary functions like GPIO and temperature readouts is outsourced
  to class AuxHwDrv.

### C++ HTTP and SSE Event Server
* [APIServer](class_a_p_i_server.html)

### Auxiliary Hardware Control and Utility Classes
* [AuxHwDrv](class_aux_hw_drv.html)
* [SensorKTY81_1xx](class_sensor_k_t_y81__1xx.html)
* [ESP32ADCChannel](class_e_s_p32_a_d_c_channel.html)
* [U16MovingAverageUInt16](class_moving_average_u_int16.html)
* [EquidistantPWLUInt16](class_equidistant_p_w_l_u_int16.html)
* [MultiTimer](class_multi_timer.html)

### Low-Level-Driver (ESP-IDF Compatible)
* See file:<br>
[components/esp32_ps_pwm/ps_pwm.h](ps__pwm_8h.html)

### Application Configuration
* [main/config/app_config.hpp](app__config_8hpp.html)


## HTTP API Documentation
### Single-page Web Application:
* Static HTTP content is served from SPI flash file system<br>
* HTTP route for static content: /s/<br>
* Path on SPI flash filesystem (SPIFFS/Littlefs): www/<br>
* Vue application sources subfolder: vue_app/

### Hardware device is controlled via HTTP GET requests:
* HTTP API endpoint:<br>
  /cmd

* Server response to control requests:<br>
  HTTP Status 200 OK and plain text content "OK"

### List of HTTP GET API requests for application control:
* Activate/deactivate the setpoint throttling / soft-start feature<br>
  /cmd?set_setpoint_throttling_enabled=(true|false)

* User setpoint limits (custom adjustment range) for output frequency [kHz]<br>
  /cmd?set_frequency_min=(float)<br>
  /cmd?set_frequency_max=(float)

* PWM output frequency setpoint [kHz]<br>
  /cmd?set_frequency=(float)

* Setpoint throttling / soft-start speed for output frequency [kHz/sec]<br>
  /cmd?set_frequency_changerate=(float)

* User setpoint limits (custom adjustment range) for PWM result duty cycle [%]<br>
  /cmd?set_duty_min=(float)<br>
  /cmd?set_duty_max=(float)

* PWM result duty cycle setpoint [%]<br>
  /cmd?set_duty=(float)

* Setpoint throttling / soft-start speed for PWM result duty cycle [kHz/sec]<br>
  /cmd?set_duty_changerate=(float)

* Dead-time setpoint for leading and lagging half-bridge leg [ns]<br>
  /cmd?set_lag_dt=(float)<br>
  /cmd?set_lead_dt=(float)

* Activate/deactivated the PWM output signal<br>
  /cmd?set_power_pwm_active=(true|false)

* Length of the power output one-shot timer pulse [sec]<br>
  /cmd?set_oneshot_len=(float)

* Trigger a one-shot output power pulse of configurable length [sec]<br>
  /cmd?trigger_oneshot[=true]

* Clear the hardware error shutdown latch<br>
  /cmd?clear_shutdown[=true]

* Power stage overcurrent limit (depends on measurement shunt value) [A]<br>
  /cmd?set_current_limit=(float)

* Overtemperature protection limits for sensor channels 1 and 2 [°C]<br>
  /cmd?set_temp_1_limit=(float)<br>
  /cmd?set_temp_2_limit=(float)

* Activate/deactivate power output relays/contactors<br>
  /cmd?set_relay_ref_active=(true|false)<br>
  /cmd?set_relay_dut_active=(true|false)

* Fan override:<br>
  // When set to "true", fan is always ON. Otherwise, fan is temperature-controlled<br>
  /cmd?set_fan_override=(true|false)

* Save all runtime settings to SPI flash for persistence accross hardware restarts<br>
  /cmd?save_settings[=true]

### Server sends periodic application status update via Server-Sent Events:
* SSE event source endpoint:<br>
/events

* Event source "app_state" telegram content is a JSON formatted data object:
```
{   
    // Setpoint throttling / soft-start feature activated/deactivated
    "setpoint_throttling_enabled":  (true|false)
    // Clock divider settings (read-only) [number factor]
    "base_div":  (uint8),
    "timer_div":  (uint8),
    // Hardware setpoint limits (maximum adjustment range) for output frequency [kHz]
    "frequency_min_hw":  (float),
    "frequency_max_hw":  (float)
    // User setpoint limits (custom adjustment range) for output frequency [kHz]
    "frequency_min":  (float)
    "frequency_max":  (float)
    // PWM output frequency setpoint [kHz]
    "frequency":  (float)
    // Setpoint throttling / soft-start speed for output frequency [kHz/sec]
    "frequency_changerate":  (float)
    // User setpoint limits (custom adjustment range) for PWM result duty cycle [%]
    "duty_min":  (float) 
    "duty_max":  (float)
    // PWM result duty cycle setpoint [%]
    "duty":  (float)
    // Setpoint throttling / soft-start speed for PWM result duty cycle [kHz/sec]
    "duty_changerate":  (float)
    // Hardware limits for dead-time adjustment [ns]. Sum of dead-times must be smaller.
    "dt_sum_max_hw":  (float)
    // Dead-time setpoint for leading and lagging half-bridge leg [ns]
    "lead_dt":  (float)
    "lag_dt":  (float)
    // Power stage overcurrent limit (depends on measurement shunt value) [A]
    "current_limit":  (float)
    // Overtemperature protection limits for sensor channels 1 and 2 [°C]
    "temp_1_limit":  (float)
    "temp_2_limit":  (float)
    // Temperature sensor readout for channels 1 and 2 [°C]
    "temp_1":  (float)
    "temp_2":  (float)
    // Heatsink fan activated/deactivated
    "fan_active":  (true|false)
    // Fan override activated/deactivated:
    // When set to "true", fan is always ON. Otherwise, fan is temperature-controlled
    "fan_override":  (true|false)
    // Power output relays on/off
    "relay_ref_active":  (true|false)
    "relay_dut_active":  (true|false)
    // Gate driver supply and disable signal status (reat-only)
    "drv_supply_active":  (true|false)
    "drv_disabled":  (true|false)
    // PWM output signal activated/deactivated
    "power_pwm_active":  (true|false)
    // Hardware Fault Shutdown Status is latched using this flag (read-only)
    "hw_oc_fault":  (true|false)
    // Overtemperature shutdown active flag (read-only)
    "hw_overtemp":  (true|false)
    // Length of the power output one-shot timer pulse [seconds]
    "oneshot_len":  (float)
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

### Screenshots
![Appliation Main Page](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/01_LiveControl.png?raw=true)

![Appliation Settings](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/02_Settings.png?raw=true)

![WiFi Network Configuration](https://github.com/ul-gh/esp_ajax_if/blob/master/doc/img/03_WiFi_Configurator.png?raw=true)

### License
[GPL v3.0](./LICENSE)
