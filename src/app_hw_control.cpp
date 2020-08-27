#include <Arduino.h>

#include "info_debug_error.h"
#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "app_hw_control.hpp"

#ifdef USE_ASYMMETRIC_FULL_SPEED_DRIVE_API
#define API_CHOICE_INIT pspwm_up_ctr_mode_init_compat
#define API_CHOICE_SET_FREQUENCY pspwm_up_ctr_mode_set_frequency
#define API_CHOICE_SET_PS_DUTY pspwm_up_ctr_mode_set_ps_duty
#define API_CHOICE_SET_DEADTIMES pspwm_up_ctr_mode_set_deadtimes_compat
#else
#define API_CHOICE_INIT pspwm_up_down_ctr_mode_init
#define API_CHOICE_SET_FREQUENCY pspwm_up_down_ctr_mode_set_frequency
#define API_CHOICE_SET_PS_DUTY pspwm_up_down_ctr_mode_set_ps_duty
#define API_CHOICE_SET_DEADTIMES pspwm_up_down_ctr_mode_set_deadtimes
#endif


PsPwmAppHwControl::PsPwmAppHwControl(APIServer* api_server)
    : api_server{api_server},
    periodic_update_timer{}
{
    debug_print("Configuring Phase-Shift-PWM...");
    esp_err_t errors = API_CHOICE_INIT(mcpwm_num,
                                       gpio_pwm0a_out, gpio_pwm0b_out,
                                       gpio_pwm1a_out, gpio_pwm1b_out,
                                       init_frequency,
                                       init_ps_duty,
                                       init_lead_dt, init_lag_dt,
                                       init_power_pwm_active,
                                       disable_action_lead_leg,
                                       disable_action_lag_leg);
    errors |= pspwm_get_setpoint_limits_ptr(mcpwm_num, &pspwm_setpoint_limits);
    errors |= pspwm_get_setpoint_ptr(mcpwm_num, &pspwm_setpoint);
    errors |= pspwm_get_clk_conf_ptr(mcpwm_num, &pspwm_clk_conf);
//    errors |= pspwm_enable_hw_fault_shutdown(mcpwm_num,
//                                             gpio_fault_shutdown,
//                                             MCPWM_LOW_LEVEL_TGR);
    if (errors != ESP_OK) {
        error_print("Error initializing the PS-PWM module!");
        return;
    }

    // Create instance of auxiliary HW control module
    debug_print("Configuring auxiliary HW control module...");
    AuxHwDrv aux_hw_drv{init_current_limit, init_relay_ref_active,
                        init_relay_dut_active, init_fan_active};
    
    register_remote_control(api_server);
    periodic_update_timer.attach_ms(api_state_periodic_update_interval_ms,
                                    on_periodic_update_timer,
                                    this);
}

PsPwmAppHwControl::~PsPwmAppHwControl() {
    periodic_update_timer.detach();
}

/* Registers hardware control function callbacks
 * as request handlers of the HTTP server
 */
void PsPwmAppHwControl::register_remote_control(APIServer* api_server) {
    CbFloatT cb_float;
    cb_float = [this](const float n) {
        pspwm_setpoint->frequency = n * 1E3;
        API_CHOICE_SET_FREQUENCY(mcpwm_num, pspwm_setpoint->frequency);
    };
    api_server->register_api_cb("set_frequency", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->ps_duty = n / 100;
        API_CHOICE_SET_PS_DUTY(mcpwm_num, pspwm_setpoint->ps_duty);
    };
    api_server->register_api_cb("set_duty", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lag_red = n * 1E-9;
        API_CHOICE_SET_DEADTIMES(mcpwm_num,
                                 pspwm_setpoint->lead_red,
                                 pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lag_dt", cb_float);

    cb_float = [this](const float n) {
        pspwm_setpoint->lead_red = n * 1E-9;
        API_CHOICE_SET_DEADTIMES(mcpwm_num,
                                 pspwm_setpoint->lead_red,
                                 pspwm_setpoint->lag_red);
    };
    api_server->register_api_cb("set_lead_dt", cb_float);

    CbStringT cb_text = [this](const String &text) {
        if (text == "true") {
            pspwm_resync_enable_output(mcpwm_num);
        } else {
            pspwm_disable_output(mcpwm_num);
        }
    };
    api_server->register_api_cb("set_power_pwm_active", cb_text);

    CbVoidT cb_void = [this](){
        if (!pspwm_get_hw_fault_shutdown_present(mcpwm_num)) {
            pspwm_clear_hw_fault_shutdown_occurred(mcpwm_num);
        } else {
            error_print("Will Not Clear: Fault Shutdown Pin Still Active!");
        }
    };
    api_server->register_api_cb("clear_shutdown", cb_void);

    //////////////// Callbacks for aux HW driver module
    cb_float = [this](const float n) {
        aux_hw_drv.set_current_limit(n);
    };
    api_server->register_api_cb("set_current_limit", cb_float);

    CbStringT cb_text = [this](const String &text) {
        aux_hw_drv.set_relay_ref_active(text == "true");
    };
    api_server->register_api_cb("set_relay_ref_active", cb_text);

    CbStringT cb_text = [this](const String &text) {
        aux_hw_drv.set_relay_dut_active(text == "true");
    };
    api_server->register_api_cb("set_relay_dut_active", cb_text);

    CbStringT cb_text = [this](const String &text) {
        aux_hw_drv.set_fan_active(text == "true");
    };
    api_server->register_api_cb("set_fan_active", cb_text);
}

// Called periodicly submitting application state to the HTTP client
void PsPwmAppHwControl::on_periodic_update_timer(PsPwmAppHwControl* self) {
    bool shutdown_status = pspwm_get_hw_fault_shutdown_occurred(mcpwm_num);
    if (shutdown_status) {
        self->pspwm_setpoint->output_enabled = false;
    }

    /* The sizes needs to be adapted accordingly if below structure
     * size is changed (!) (!!)
     */
    constexpr size_t json_object_size = JSON_OBJECT_SIZE(15);
    constexpr size_t strings_size = sizeof(
        "frequency_min""frequency_max""dt_sum_max"
        "frequency""duty""lead_dt""lag_dt""power_pwm_active"
        "current_limit""relay_ref_active""relay_dut_active""fan_active"
        "base_div""timer_div""hw_shutdown_active"
        );
    constexpr size_t I_AM_SCARED_MARGIN = 50;
    // ArduinoJson JsonDocument object, see https://arduinojson.org
    StaticJsonDocument<json_object_size> json_doc;
    // Setpoint limits
    json_doc["frequency_min"] = self->pspwm_setpoint_limits->frequency_min;
    json_doc["frequency_max"] = self->pspwm_setpoint_limits->frequency_max;
    json_doc["dt_sum_max"] = self->pspwm_setpoint_limits->dt_sum_max;
    // Operational setpoints for PSPWM module
    json_doc["frequency"] = self->pspwm_setpoint->frequency;
    json_doc["duty"] = self->pspwm_setpoint->ps_duty;
    json_doc["lead_dt"] = self->pspwm_setpoint->lead_red;
    json_doc["lag_dt"] = self->pspwm_setpoint->lag_red;
    json_doc["power_pwm_active"] = self->pspwm_setpoint->output_enabled;
    // Settings for auxiliary HW control module
    json_doc["current_limit"] = self->aux_hw_drv.current_limit;
    json_doc["relay_ref_active"] = self->aux_hw_drv.relay_ref_active;
    json_doc["relay_dut_active"] = self->aux_hw_drv.relay_dut_active;
    json_doc["fan_active"] = self->aux_hw_drv.fan_active;

    // Clock divider settings
    json_doc["base_div"] = self->pspwm_clk_conf->base_clk_prescale;
    json_doc["timer_div"] = self->pspwm_clk_conf->timer_clk_prescale;
    // Hardware Fault Shutdown Status
    json_doc["hw_shutdown_active"] = shutdown_status;

    char json_str_buffer[json_object_size + strings_size + I_AM_SCARED_MARGIN];
    serializeJson(json_doc, json_str_buffer);
    self->api_server->event_source->send(json_str_buffer, "app_state");
}