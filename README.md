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
    // Setpoint limits
    "frequency_min": 1.0,
    "frequency_max": 1000.0,
    "dt_sum_max": 1200,

    // Operational settings
    "frequency": 500.0,
    "ps_duty": 79.0,
    "lead_dt": 100.0,
    "lag_dt": 200.0,
    "output_enabled": true,

    // Clock divider settings
    "base_div": 1,
    "timer_div": 1,

    // Hardware Fault Shutdown Status
    "hw_shutdown_active": false,
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
