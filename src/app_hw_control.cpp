#include <Arduino.h>

#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "app_hw_control.h"

PSPWMGen::PSPWMGen(HTTPServer &http_server) {
    Serial.println("Configuring Phase-Shift-PWM...");
    // http_server = http_server;
    pspwm_up_ctr_mode_init(
        MCPWM_NUM,
        GPIO_PWM0A_OUT, GPIO_PWM0B_OUT, GPIO_PWM1A_OUT, GPIO_PWM1B_OUT,
        frequency,
        ps_duty,
        lead_dt, lead_dt, lag_dt, lag_dt);
    register_remote_control(http_server);
}

/* Registers hardware control function callbacks
 * as request handlers of the HTTP server
 */
void PSPWMGen::register_remote_control(HTTPServer &http_server) {
    http_server.register_command("set_frequency", [this](float n) {
        this->frequency = n * 1E3;
        pspwm_up_ctr_mode_set_frequency(MCPWM_NUM, frequency);
    });

    http_server.register_command("set_duty", [this](float n) {
        this->ps_duty = n / 100;
        pspwm_up_ctr_mode_set_ps_duty(MCPWM_NUM, ps_duty);
    });

    http_server.register_command("lag_dt", [this](float n) {
        this->lag_dt = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(MCPWM_NUM,
                                        lead_dt, lead_dt, lag_dt, lag_dt);
    });

    http_server.register_command("lead_dt", [this](float n) {
        this->lead_dt = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(MCPWM_NUM,
                                        lead_dt, lead_dt, lag_dt, lag_dt);
    });

    http_server.register_command("set_output", [this](const String &text) {
        this->output_enabled = text == "ON";
        if (output_enabled) {
            pspwm_resync_enable_output(MCPWM_NUM);
        } else {
            pspwm_disable_output(MCPWM_NUM);
        }
    });
}

