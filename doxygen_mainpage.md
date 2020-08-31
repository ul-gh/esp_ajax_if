\mainpage
# ESP32_PSPWM
Driver for the MCPWM hardware modules on the Espressif ESP32 SoC for
generating a Phase-Shift-PWM waveform between two pairs of hardware pins.

Application in power electronics, e.g. Zero-Voltage-Switching (ZVS) Full-Bridge-,
Dual-Active-Bridge- and LLC converters.

## Low-Level-Driver (ESP-IDF compatible)
* See file:<br>
[src/ps_pwm.h](ps__pwm_8h.html)

## C++ API
* See class reference:<br>
[PSPWMGen](class_p_s_p_w_m_gen.html) (documentation is work-in-progress)

## HTTP API Documentation

### Application is AJAX with static HTML5 + CSS + JavaScript ES7
* Static HTTP content is served from SPI flash file system path in source tree:<br>
/data/www

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

* Activate/deactivate the output:<br>
/cmd?set_output=off

* Clear error shutdown condition:<br>
/cmd?clear_shutdown


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
    "power_pwm_active": Self-Explanatory, Boolean JSON..
    // Settings for auxiliary HW control module
    "current_limit": Current limit in Amperes
    "relay_ref_active": Self-Explanatory, Boolean JSON..
    "relay_dut_active": Self-Explanatory, Boolean JSON..
    "fan_active": Self-Explanatory, Boolean..

    // Clock divider settings
    "base_div": Integer clock divider factor
    "timer_div": ditto.
    // Gate driver supply and disable signals
    "driver_supply_active": Self-Explanatory, Boolean JSON..
    "drv_disabled": Self-Explanatory, Boolean..
    // Hardware Fault Shutdown Status
    "hw_shutdown_active: Self-Explanatory, Boolean JSON..
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
[GPL v3.0](license_gplv3.txt)
