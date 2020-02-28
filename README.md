\mainpage
# ESP32_PSPWM
Driver for the MCPWM hardware modules on the Espressif ESP32 SoC for
generating a Phase-Shift-PWM waveform between two pairs of hardware pins.

Application in power electronics, e.g. Zero-Voltage-Switching (ZVS) Full-Bridge-,
Dual-Active-Bridge- and LLC converters.

## Low-Level-Driver (ESP-IDF compatible)
See file: src/ps_pwm.h

## C++ API
See class reference: PSPWMGen (documentation is work-in-progress)

## HTTP API Documentation

### Application is AJAX with static HTML5 + CSS + JavaScript ES7
* Static HTTP content is served from SPI flashh file system path in source tree:  
/data/www

### Hardware device is controlled via HTTP GET requests:
* HTTP API endpoint:  
/cmd

* Server response to control requests:  
HTTP Status 200 OK and plain text content "OK"

### List of HTTP GET requests for application control:

* Set frequency in kHz:  
/cmd?set_frequency=500

* Set bridge output resulting duty cycle in %:  
/cmd?set_duty=45.3

* set lead bridge leg dead time in nanoseconds:  
/cmd?set_lead_dt=300

* set lag bridge leg dead time in nanoseconds:  
/cmd?set_lag_dt=300

* Activate/deactivate the output:  
/cmd?set_output=off

* Clear error shutdown condition:  
/cmd?clear_shutdown


### Server sends periodic application status update via Server-Sent Events:
* SSE event source endpoint:  
/events

* Event source telegram content is JSON with content:
```
{
    // Setpoint limits
    "frequency_min": 1.0,
    "frequency_max": 1000.0,

    // Operational settings
    "dt_sum_max": 1200,
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

### Example application control from PC side (Python):
```
import requests

url = "http://kalorimeter.kostal.int/cmd"

cmd1 = {"set_frequency": 500}
cmd2 = {"set_duty": 45.3}

requests.get(url, cmd1)
requests.get(url, cmd2)
…
```

## License
[GPL v3.0](LICENSE)
