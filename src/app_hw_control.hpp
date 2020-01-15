#ifndef APP_HW_CONTROL_HPP__
#define APP_HW_CONTROL_HPP__

#include "driver/mcpwm.h"
#include "http_server.h"

// MCPWM unit can be [0,1]
constexpr mcpwm_unit_t mcpwm_num = MCPWM_UNIT_0;
// GPIO config
constexpr int gpio_pwm0a_out = 27; // PWM0A := LEAD leg, Low Side
constexpr int gpio_pwm0b_out = 26; // PWM0B := LEAD leg, High Side
constexpr int gpio_pwm1a_out = 25; // PWM1A := LAG leg, Low Side
constexpr int gpio_pwm1b_out = 33; // PWM1B := LAG leg, High Side
constexpr int gpio_fault_shutdown = 4; // Shutdown/fault input for PWM outputs

class PSPWMGen
/* Preset runtime values configuration:
 *
 * Frequency 20 kHz,
 * phase-shift duty cycle of 45% per output or 90% of rectified waveform,
 * dead-time 300 ns both bridge outputs.
 */
{
public:
    float frequency{100e3};
    float ps_duty{0.45};
    float lead_dt{125e-9};
    float lag_dt{125e-9};
    bool output_enabled{false};

    PSPWMGen(HTTPServer &http_server);
    // virtual ~PSPWMGen();

    // Register hw control functions as request handlers with the HTPP server
    void register_remote_control(HTTPServer &http_server);
};

#endif