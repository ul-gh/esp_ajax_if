#ifndef __PS_PWM_GEN_H__
#define __PS_PWM_GEN_H__

#include "http_server.h"

// MCPWM unit can be [0,1]
#define MCPWM_NUM MCPWM_UNIT_0
// GPIO config
#define GPIO_PWM0A_OUT 0 // PWM0A := LEAD LEG, HB2_HS
#define GPIO_PWM0B_OUT 4 // PWM0B := LEAD LEG, HB2_LS
#define GPIO_PWM1A_OUT 16 // PWM1A := LAG LEG, HB1_HS
#define GPIO_PWM1B_OUT 17 // PWM1B := LAG LEG, HB1_LS

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