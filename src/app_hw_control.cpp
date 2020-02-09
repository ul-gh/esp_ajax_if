#include <Arduino.h>

#include "info_debug_error.h"
#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "app_hw_control.hpp"

#ifdef USE_ASYMMETRIC_FULL_SPEED_DRIVE_API
PSPWMGen::PSPWMGen(APIServer* api_server)
    : api_server{api_server},
    push_update_timer{}
{
    debug_print("Configuring Phase-Shift-PWM...");
    // http_server = http_server;
    bool is_ok = pspwm_up_ctr_mode_init(
            mcpwm_num,
            gpio_pwm0a_out, gpio_pwm0b_out,
            gpio_pwm1a_out, gpio_pwm1b_out,
            init_frequency,
            init_ps_duty,
            init_lead_dt, init_lead_dt, init_lag_dt, init_lag_dt,
            init_output_enabled,
            disable_action_lag_leg, disable_action_lead_leg) == ESP_OK;
    is_ok &= pspwm_get_setpoint_ptr(mcpwm_num, &pspwm_setpoint) == ESP_OK;
    is_ok &= pspwm_get_setpoint_limits_ptr(mcpwm_num, &pspwm_setpoint_limits) == ESP_OK;
    if (!is_ok) {
        error_print("Error initializing the PS-PWM module!");
        return;
    }
    //pspwm_enable_hw_fault_shutdown(mcpwm_num, gpio_fault_shutdown, MCPWM_LOW_LEVEL_TGR);
    register_remote_control(api_server);
    push_update_timer.attach_ms(api_state_push_update_interval_ms,
                                on_push_update_timer,
                                this);
}

/* Registers hardware control function callbacks
 * as request handlers of the HTTP server
 */
void PSPWMGen::register_remote_control(APIServer* api_server) {
    CbFloatT cb_float;
    cb_float = [this](const float n) {
        pspwm_setpoint->frequency = n * 1E3;
        pspwm_up_ctr_mode_set_frequency(mcpwm_num, pspwm_setpoint->frequency);
    };
    api_server->register_api_cb("set_frequency", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->ps_duty = n / 100;
        pspwm_up_ctr_mode_set_ps_duty(mcpwm_num, pspwm_setpoint->ps_duty);
    };
    api_server->register_api_cb("set_duty", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lag_red = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(mcpwm_num,
                                        pspwm_setpoint->lead_red,
                                        pspwm_setpoint->lead_red,
                                        pspwm_setpoint->lag_red,
                                        pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lag_dt", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lead_red = n * 1E-9;
        pspwm_up_ctr_mode_set_deadtimes(mcpwm_num,
                                        pspwm_setpoint->lead_red,
                                        pspwm_setpoint->lead_red,
                                        pspwm_setpoint->lag_red,
                                        pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lead_dt", cb_float);

    CbStringT cb_text = [this](const String &text) {
        pspwm_setpoint->output_enabled = text == "ON";
        if (pspwm_setpoint->output_enabled) {
            pspwm_resync_enable_output(mcpwm_num);
        } else {
            pspwm_disable_output(mcpwm_num);
        }
    };
    api_server->register_api_cb("set_output", cb_text);
}
#endif /* USE_ASYMMETRIC_FULL_SPEED_DRIVE_API */

#ifdef USE_SYMMETRIC_DC_FREE_DRIVE_API
PSPWMGen::PSPWMGen(APIServer* api_server)
    : api_server{api_server},
    push_update_timer{}
{
    debug_print("Configuring Phase-Shift-PWM...");
    // http_server = http_server;
    bool is_ok = pspwm_up_down_ctr_mode_init(
            mcpwm_num,
            gpio_pwm0a_out, gpio_pwm0b_out,
            gpio_pwm1a_out, gpio_pwm1b_out,
            init_frequency,
            init_ps_duty,
            init_lead_dt, init_lag_dt,
            init_output_enabled,
            disable_action_lag_leg, disable_action_lead_leg) == ESP_OK;
    is_ok &= pspwm_get_setpoint_limits_ptr(mcpwm_num, &pspwm_setpoint_limits) == ESP_OK;
    is_ok &= pspwm_get_setpoint_ptr(mcpwm_num, &pspwm_setpoint) == ESP_OK;
    is_ok &= pspwm_get_clk_conf_ptr(mcpwm_num, &pspwm_clk_conf) == ESP_OK;
    is_ok &= pspwm_enable_hw_fault_shutdown(mcpwm_num, gpio_fault_shutdown, MCPWM_LOW_LEVEL_TGR);
    if (!is_ok) {
        error_print("Error initializing the PS-PWM module!");
        return;
    }
    register_remote_control(api_server);
    push_update_timer.attach_ms(api_state_push_update_interval_ms,
                                on_push_update_timer,
                                this);
}

/* Registers hardware control function callbacks
 * as request handlers of the HTTP server
 */
void PSPWMGen::register_remote_control(APIServer* api_server) {
    CbFloatT cb_float;
    cb_float = [this](const float n) {
        pspwm_setpoint->frequency = n * 1E3;
        pspwm_up_down_ctr_mode_set_frequency(mcpwm_num, pspwm_setpoint->frequency);
    };
    api_server->register_api_cb("set_frequency", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->ps_duty = n / 100;
        pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_num, pspwm_setpoint->ps_duty);
    };
    api_server->register_api_cb("set_duty", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lag_red = n * 1E-9;
        pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_num,
                                             pspwm_setpoint->lead_red,
                                             pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lag_dt", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lead_red = n * 1E-9;
        pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_num,
                                             pspwm_setpoint->lead_red,
                                             pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lead_dt", cb_float);

    CbStringT cb_text = [this](const String &text) {
        pspwm_setpoint->output_enabled = text == "ON";
        if (pspwm_setpoint->output_enabled) {
            pspwm_resync_enable_output(mcpwm_num);
        } else {
            pspwm_disable_output(mcpwm_num);
        }
    };
    api_server->register_api_cb("set_output", cb_text);
}
#endif /* USE_SYMMETRIC_DC_FREE_DRIVE_API */

PSPWMGen::~PSPWMGen() {
    push_update_timer.detach();
}

// Called periodicly submitting application state to the HTTP client
void PSPWMGen::on_push_update_timer(PSPWMGen* self) {
    StaticJsonDocument<json_object_size> app_state_json;
    // Setpoint limits
    app_state_json["frequency_min"] = self->pspwm_setpoint_limits->frequency_min;
    app_state_json["frequency_max"] = self->pspwm_setpoint_limits->frequency_max;
    app_state_json["dt_sum_max"] = self->pspwm_setpoint_limits->dt_sum_max;
    // Operational settings
    app_state_json["frequency"] = self->pspwm_setpoint->frequency;
    app_state_json["ps_duty"] = self->pspwm_setpoint->ps_duty;
    app_state_json["lead_dt"] = self->pspwm_setpoint->lead_red;
    app_state_json["lag_dt"] = self->pspwm_setpoint->lag_red;
    app_state_json["output_enabled"] = self->pspwm_setpoint->output_enabled;
    // Clock divider settings
    app_state_json["base_div"] = self->pspwm_clk_conf->base_clk_prescale;
    app_state_json["timer_div"] = self->pspwm_clk_conf->timer_clk_prescale;

    serializeJson(app_state_json, Serial);
}