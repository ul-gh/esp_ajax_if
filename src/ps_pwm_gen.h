#ifndef __PS_PWM_GEN_H__
#define __PS_PWM_GEN_H__

#include "driver/mcpwm.h"
#include "ps_pwm_lowlevel.h"

#define GPIO_PWM0A_OUT 19 //Set GPIO 19 as PWM0A, first half-bridge
#define GPIO_PWM0B_OUT 18 //Set GPIO 18 as PWM0B, first half-bridge
#define GPIO_PWM1A_OUT 17 //Set GPIO 17 as PWM1A, second half-bridge
#define GPIO_PWM1B_OUT 16 //Set GPIO 16 as PWM1B, second half-bridge

class PSPWMGen
/* Preset configuration:
 *
 * Frequency 20 kHz,
 * phase-shift duty cycle of 45% per output or 90% of rectified waveform,
 * dead-time 300 ns both bridge outputs.
 */
{
public:
    float frequency{20e3};
    float phase_shift{0.45};
    float lead_dt{300e-9};
    float lag_dt{300e-9};
    bool output_on{false};

    mcpwm_unit_t pwm_unit{MCPWM_UNIT_0};

    uint32_t gpio_lead_a{GPIO_PWM0A_OUT};
    uint32_t gpio_lead_b{GPIO_PWM0B_OUT};
    uint32_t gpio_lag_a{GPIO_PWM1A_OUT};
    uint32_t gpio_lag_b{GPIO_PWM1B_OUT};

    PSPWMGen();
    // virtual ~PSPWMGen();

    void update_gpios();
    void set_frequency(float frequency);
    void set_phase_shift(float phase_shift);
    void set_lead_dt(float lead_dt);
    void set_lag_dt(float lag_dt);
};

#endif