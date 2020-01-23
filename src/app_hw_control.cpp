#include <Arduino.h>

#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "app_hw_control.hpp"

PSPWMGen::PSPWMGen(HTTPServer &http_server) {
    Serial.println("Configuring Phase-Shift-PWM...");
    // http_server = http_server;
    pspwm_up_ctr_mode_init(mcpwm_num,
                           gpio_pwm0a_out, gpio_pwm0b_out,
                           gpio_pwm1a_out, gpio_pwm1b_out,
                           frequency,
                           ps_duty,
                           lead_dt, lead_dt, lag_dt, lag_dt,
                           disable_action_lag_leg, disable_action_lead_leg);
    //pspwm_enable_hw_fault_shutdown(mcpwm_num, gpio_fault_shutdown, MCPWM_LOW_LEVEL_TGR);
    register_remote_control(http_server);
}

/* Registers hardware control function callbacks
 * as request handlers of the HTTP server
 */
void PSPWMGen::register_remote_control(HTTPServer &http_server) {
    http_server.register_command("set_frequency", [this](float n) {
        frequency = n * 1E3;
        pspwm_up_ctr_mode_set_frequency(mcpwm_num, frequency);
    });

    http_server.register_command("set_duty", [this](float n) {
        ps_duty = n / 100;
        pspwm_up_ctr_mode_set_ps_duty(mcpwm_num, ps_duty);
    });

    http_server.register_command("set_lag_dt", [this](float n) {
        lag_dt = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(mcpwm_num,
                                        lead_dt, lead_dt, lag_dt, lag_dt);
    });

    http_server.register_command("set_lead_dt", [this](float n) {
        lead_dt = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(mcpwm_num,
                                        lead_dt, lead_dt, lag_dt, lag_dt);
    });

    http_server.register_command("set_output", [this](const String &text) {
        output_enabled = text == "ON";
        if (output_enabled) {
            pspwm_resync_enable_output(mcpwm_num);
        } else {
            pspwm_disable_output(mcpwm_num);
        }
    });
}

