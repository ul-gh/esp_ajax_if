#include <Arduino.h>

#include "ps_pwm_gen.h"

PSPWMGen::PSPWMGen() {
    Serial.println("Configuring Phase-Shift-PWM...");
    update_gpios();
}

void PSPWMGen::update_gpios() {
    pspwm_setup_gpios(
        pwm_unit, gpio_lead_a, gpio_lead_b, gpio_lag_a, gpio_lag_b);
}

void PSPWMGen::set_frequency(float frequency) {
    pspwm_init_individual_deadtimes(
        pwm_unit, frequency, phase_shift, lead_dt, lead_dt, lag_dt, lag_dt);
}

void PSPWMGen::set_phase_shift(float phase_shift) {
    pspwm_init_individual_deadtimes(
        pwm_unit, frequency, phase_shift, lead_dt, lead_dt, lag_dt, lag_dt);
}

void PSPWMGen::set_lead_dt(float lead_dt) {
    pspwm_init_individual_deadtimes(
        pwm_unit, frequency, phase_shift, lead_dt, lead_dt, lag_dt, lag_dt);
}

void PSPWMGen::set_lag_dt(float lag_dt) {
    pspwm_init_individual_deadtimes(
        pwm_unit, frequency, phase_shift, lead_dt, lead_dt, lag_dt, lag_dt);
}
