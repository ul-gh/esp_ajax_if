#ifndef __PS_PWM_GEN_H__
#define __PS_PWM_GEN_H__

#include "http_server.h"

// MCPWM unit can be [0,1]
#define MCPWM_NUM MCPWM_UNIT_0
// GPIO config
#define GPIO_PWM0A_OUT 27 // PWM0A := LEAD leg, Low Side
#define GPIO_PWM0B_OUT 26 // PWM0B := LEAD leg, High Side
#define GPIO_PWM1A_OUT 25 // PWM1A := LAG leg, Low Side
#define GPIO_PWM1B_OUT 33 // PWM1B := LAG leg, High Side

class PSPWMGen
/* Preset runtime values configuration:
 *
 * Frequency 20 kHz,
 * phase-shift duty cycle of 45% per output or 90% of rectified waveform,
 * dead-time 300 ns both bridge outputs.
 */
{
public:
    float frequency{20e3};
    float ps_duty{0.45};
    float lead_dt{300e-9};
    float lag_dt{300e-9};
    bool output_enabled{false};

    PSPWMGen(HTTPServer &http_server);
    // virtual ~PSPWMGen();

    // Register hw control functions as request handlers with the HTPP server
    void register_remote_control(HTTPServer &http_server);
};

#endif