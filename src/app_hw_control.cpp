/* Application interface implementation for PS-PWM generator hardware
 *
 * This features the main control functions for PWM frequency, duty cycle etc.
 * 
 * Also, periodic state feedback for all hardware functions is sent to the
 * HTTP remote interface using Server-Sent Events from a FreeRTOS timer task.
 * 
 * Some auxiliary functions like GPIO and temperature readouts is outsourced
 * to the AuxHwDrv class, see aux_hw_drv.cpp.
 * 
 * License: GPL v.3 
 * U. Lukas 2020-09-27
 */
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/mcpwm.h"
#include <Arduino.h>

#include "ps_pwm.h"
#include "app_hw_control.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "app_hw_control.cpp";

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


void PsPwmAppState::serialize() {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
    // ArduinoJson JsonDocument object, see https://arduinojson.org
    StaticJsonDocument<_json_objects_size> json_doc;
    // Setpoint limits from PSPWM hw constraints. Scaled to kHz, ns and % respectively...
    json_doc["frequency_min_hw"] = pspwm_setpoint_limits->frequency_min/1e3;
    json_doc["frequency_max_hw"] = pspwm_setpoint_limits->frequency_max/1e3;
    json_doc["dt_sum_max_hw"] = pspwm_setpoint_limits->dt_sum_max*1e9;
    // Runtime user setpoint limits for output frequency
    json_doc["frequency_min"] = frequency_min/1e3;
    json_doc["frequency_max"] = frequency_max/1e3;
    // Operational setpoints for PSPWM module
    json_doc["frequency"] = pspwm_setpoint->frequency/1e3;
    json_doc["duty"] = pspwm_setpoint->ps_duty*100;
    json_doc["lead_dt"] = pspwm_setpoint->lead_red*1e9;
    json_doc["lag_dt"] = pspwm_setpoint->lag_red*1e9;
    json_doc["power_pwm_active"] = pspwm_setpoint->output_enabled;
    // Settings for auxiliary HW control module
    json_doc["current_limit"] = aux_hw_drv_state->current_limit;
    json_doc["relay_ref_active"] = aux_hw_drv_state->relay_ref_active;
    json_doc["relay_dut_active"] = aux_hw_drv_state->relay_dut_active;
    // Temperatures and fan
    json_doc["aux_temp"] = aux_hw_drv_state->aux_temp;
    json_doc["heatsink_temp"] = aux_hw_drv_state->heatsink_temp;
    json_doc["fan_active"] = aux_hw_drv_state->fan_active;
    // Clock divider settings
    json_doc["base_div"] = pspwm_clk_conf->base_clk_prescale;
    json_doc["timer_div"] = pspwm_clk_conf->timer_clk_prescale;
    // Gate driver supply and disable signals
    json_doc["drv_supply_active"] = aux_hw_drv_state->drv_supply_active;
    json_doc["drv_disabled"] = aux_hw_drv_state->drv_disabled;
    // True when hardware OC shutdown condition is present
    json_doc["hw_oc_fault_present"] = hw_oc_fault_present;
    // Hardware Fault Shutdown Status is latched using this flag
    json_doc["hw_oc_fault_occurred"] = hw_oc_fault_occurred;
    // Do the serialization
    serializeJson(json_doc, json_buf);
}


struct Event
{
    enum {TIMER_FAST, TIMER_SLOW, STATE_CHANGED, CONFIG_CHANGED};
    static constexpr EventBits_t timer_fast{1<<TIMER_FAST};
    static constexpr EventBits_t timer_slow{1<<TIMER_SLOW};
    static constexpr EventBits_t state_changed{1<<STATE_CHANGED};
    static constexpr EventBits_t config_changed{1<<CONFIG_CHANGED};

    const EventBits_t value;

    Event(EventBits_t event_bitset)
        : value{event_bitset}
    {}

    /** Returns true if event value has all bits set at input bitmask position.
     * Bitmask values are defined herein.
     */
    bool has(EventBits_t bitmask) const {
        return (value & bitmask) == bitmask;
    }
};


// FreeRTOS task handle for application event task
TaskHandle_t PsPwmAppHwControl::_app_event_task_handle;
// FreeRTOS event group handle for triggering event task actions
EventGroupHandle_t PsPwmAppHwControl::_app_event_group;

PsPwmAppHwControl::PsPwmAppHwControl(APIServer* api_server)
    :
    // HTTP AJAX API server instance was created before
    api_server{api_server}
{
    assert(api_server);
    // Reads this.app_conf and sets this.state
    _initialize_ps_pwm_drv();
    state.aux_hw_drv_state = &aux_hw_drv.state;
    _create_app_event_task();
}

PsPwmAppHwControl::~PsPwmAppHwControl() {
    event_timer.detach();
    //xTimerDelete(event_timer, 0);
}

void PsPwmAppHwControl::_initialize_ps_pwm_drv() {
    ESP_LOGI(TAG, "Configuring Phase-Shift-PWM...");
    esp_err_t errors = API_CHOICE_INIT(
        app_conf.mcpwm_num,
        app_conf.gpio_pwm0a_out, app_conf.gpio_pwm0b_out,
        app_conf.gpio_pwm1a_out, app_conf.gpio_pwm1b_out,
        app_conf.init_frequency, app_conf.init_ps_duty,
        app_conf.init_lead_dt, app_conf.init_lag_dt,
        app_conf.init_power_pwm_active,
        app_conf.disable_action_lead_leg, app_conf.disable_action_lag_leg);
    errors |= pspwm_get_setpoint_limits_ptr(app_conf.mcpwm_num,
                                            &(state.pspwm_setpoint_limits));
    errors |= pspwm_get_setpoint_ptr(app_conf.mcpwm_num, &(state.pspwm_setpoint));
    errors |= pspwm_get_clk_conf_ptr(app_conf.mcpwm_num, &(state.pspwm_clk_conf));
    errors |= pspwm_enable_hw_fault_shutdown(app_conf.mcpwm_num,
                                             app_conf.gpio_fault_shutdown,
                                             MCPWM_LOW_LEVEL_TGR);
    // Pull-down enabled for low-level shutdown active default state
    errors |= gpio_pulldown_en((gpio_num_t)app_conf.gpio_fault_shutdown);
    if (errors != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing the PS-PWM module!");
        abort();
    }
}

void PsPwmAppHwControl::_create_app_event_task() {
    xTaskCreatePinnedToCore(_app_event_task,
                            "app_event_task", 
                            app_conf.app_event_task_stack_size,
                            static_cast<void*>(this),
                            app_conf.app_event_task_priority,
                            &_app_event_task_handle,
                            app_conf.app_event_task_core_id);
    _app_event_group = xEventGroupCreate();
    if (!_app_event_task_handle || !_app_event_group) {
        ESP_LOGE(TAG, "Failed to create application event task or event group!");
        abort();
    }
}

/** Begin operation.
 * This also starts the timer callbacks etc.
 * This will fail if networking etc. is not set up correctly!
 */
void PsPwmAppHwControl::begin() {
    ESP_LOGD(TAG, "Activating Gate driver power supply...");
    aux_hw_drv.set_drv_supply_active("true");
    _register_remote_control(api_server);
    // Configure timers triggering periodic events.
    // Fast events are used for triggering ADC conversion etc.
    event_timer.attach_ms(
        app_conf.timer_fast_interval_ms,
        [](){xEventGroupSetBits(_app_event_group, Event::timer_fast);}
    );
    // Slow events are used for sending periodic SSE push messages updating the
    // application state as displayed by the remote clients
    event_timer.attach_ms(
        app_conf.timer_slow_interval_ms,
        [](){xEventGroupSetBits(_app_event_group, Event::timer_slow);}
    );
    // Alternatively use FreeRTOS timer instead of ESP HW timer
    //event_timer = xTimerCreate(
    //    "Event timer",
    //    pdMS_TO_TICKS(app_conf.timer_fast_interval_ms),
    //    pdTRUE,
    //    NULL,
    //    [](){xEventGroupSetBits(_app_event_group, Event::timer_fast);}
    //);
    //if (!xTimerStart(event_timer, pdMS_TO_TICKS(5000))) {
    //    ESP_LOGE(TAG, "Error initializing the event timer!");
    //    abort();
    //}
}

/** AppHwControl application event task
 */
void PsPwmAppHwControl::_app_event_task(void *pVParameters) {
    auto self = static_cast<PsPwmAppHwControl*>(pVParameters);
    ESP_LOGD(TAG, "Starting PsPwmAppHwControl event task");
    // Main application event loop
    while (true) {
        const Event event = xEventGroupWaitBits(
            _app_event_group, 0xFF, pdTRUE, pdFALSE, portMAX_DELAY);
        if (event.has(Event::timer_slow)) {
            self->_push_state_update();
        }
        if (event.has(Event::state_changed)) {
            self->_push_state_update();
        }
    }
}

/** Application state is sent as a push update via the SSE event source.
 *  See file: app_hw_control.cpp
 */
void PsPwmAppHwControl::_send_state_changed_event() {
    xEventGroupSetBits(_app_event_group, Event::state_changed);
}

/* Send SSE push update to all connected clients.
 * 
 * Called periodicly (default once per second) but also asynchronously
 * on demand when state_change event is received.
 */
void PsPwmAppHwControl::_push_state_update() {
    // Variant using FreeRTOS timer
    //void PsPwmAppHwControl::_on_periodic_state_update(TimerHandle_t xTimer) {
    //    auto self = static_cast<PsPwmAppHwControl*>(pvTimerGetTimerID(xTimer));
    assert(api_server && api_server->event_source);
    // True when hardware OC shutdown condition is present
    state.hw_oc_fault_present = pspwm_get_hw_fault_shutdown_present(app_conf.mcpwm_num);
    // Hardware Fault Shutdown Status is latched using this flag
    state.hw_oc_fault_occurred = pspwm_get_hw_fault_shutdown_occurred(app_conf.mcpwm_num);
    // Update temperature sensor values on this occasion
    aux_hw_drv.update_temperature_sensors();

    // Prepare JSON message for sending
    state.serialize();
    api_server->event_source->send(state.json_buf, "hw_app_state");
}


/* Register all application HTTP GET API callbacks into the HTPP server
 */
void PsPwmAppHwControl::_register_remote_control(APIServer* api_server) {
    CbVoidT cb_void;
    CbFloatT cb_float;
    CbStringT cb_text;

    // set_frequency_min
    cb_float = [this](const float n) {
        ESP_LOGD(TAG, "Request for set_frequency_min received with value: %f", n);
        state.frequency_min = n * 1E3;
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_frequency_min", cb_float);

    // set_frequency_max
    cb_float = [this](const float n) {
        ESP_LOGD(TAG, "Request for set_frequency_max received with value: %f", n);
        state.frequency_max = n * 1E3;
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_frequency_max", cb_float);

    // set_frequency
    cb_float = [this](const float n) {
        state.pspwm_setpoint->frequency = n * 1E3;
        API_CHOICE_SET_FREQUENCY(app_conf.mcpwm_num, state.pspwm_setpoint->frequency);
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_frequency", cb_float);

    // set_duty
    cb_float = [this](const float n) {
        state.pspwm_setpoint->ps_duty = n / 100;
        API_CHOICE_SET_PS_DUTY(app_conf.mcpwm_num, state.pspwm_setpoint->ps_duty);
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_duty", cb_float);

    // set_lag_dt
    cb_float = [this](const float n) {
        state.pspwm_setpoint->lag_red = n * 1E-9;
        API_CHOICE_SET_DEADTIMES(app_conf.mcpwm_num,
                                 state.pspwm_setpoint->lead_red,
                                 state.pspwm_setpoint->lag_red);
    _send_state_changed_event();
    };
    api_server->register_api_cb("set_lag_dt", cb_float);

    // set_lead_dt
    cb_float = [this](const float n) {
        state.pspwm_setpoint->lead_red = n * 1E-9;
        API_CHOICE_SET_DEADTIMES(app_conf.mcpwm_num,
                                 state.pspwm_setpoint->lead_red,
                                 state.pspwm_setpoint->lag_red);
    _send_state_changed_event();
    };
    api_server->register_api_cb("set_lead_dt", cb_float);

    // set_power_pwm_active
    cb_text = [this](const String &text) {
        if (text == "true") {
            // FIXME: Hardware has redundant latch but no separate oc detect line.
            //        So this currently does not recognize if error is present or only latched.
            // aux_hw_drv.set_drv_disabled(false);
            pspwm_resync_enable_output(app_conf.mcpwm_num);
        } else {
            // FIXME: Same as above
            // aux_hw_drv.set_drv_disabled(true);
            pspwm_disable_output(app_conf.mcpwm_num);
        }
    _send_state_changed_event();
    };
    api_server->register_api_cb("set_power_pwm_active", cb_text);

    // clear_shutdown
    /* Callback hell.. If HW overcurrent fault pin is cleared, we first reset
     * the external hardware latch via aux_hw_drv.reset_oc_detect_shutdown()
     * which receives as additional callback a lambda containing a call to
     * pspwm_clear_hw_fault_shutdown_occurred() which on completion of
     * hardware latch reset (1 ms delay via RTOS timer) then resets the PSPWM
     * fault shutdown latch inside the SOC..
     */
    cb_void = [this](){
        // FIXME: Hardware has redundant latch but no separate oc detect line.
        //        So this currently does not recognize if error is present or only latched.
        // if (!pspwm_get_hw_fault_shutdown_present(mcpwm_num)) {
        if (true) {
            aux_hw_drv.reset_oc_shutdown_start();
            event_timer.once_ms(
                1 * aux_hw_drv.aux_hw_conf.oc_reset_pulse_length_ms,
                aux_hw_drv.reset_oc_shutdown_finish);
            event_timer.once_ms(
                2 * aux_hw_drv.aux_hw_conf.oc_reset_pulse_length_ms,
                [](){
                    ESP_LOGD(TAG, "External HW latch reset done. Resetting PSPWM SOC latch...");
                    pspwm_clear_hw_fault_shutdown_occurred(app_conf.mcpwm_num);
                    _send_state_changed_event();
                    }
                );
        } else {
            ESP_LOGE(TAG, "Will Not Clear: Fault Shutdown Pin Still Active!");
        }
    };
    api_server->register_api_cb("clear_shutdown", cb_void);

    //////////////// Callbacks for aux HW driver module
    // set_current_limit
    cb_float = [this](const float n) {
        ESP_LOGD(TAG, "Request for set_current_limit received with value: %f", n);
        aux_hw_drv.set_current_limit(n);
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_current_limit", cb_float);

    // set_relay_ref_active
    cb_text = [this](const String &text) {
        ESP_LOGD(TAG, "Request for set_relay_ref_active received!");
        aux_hw_drv.set_relay_ref_active(text == "true");
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_relay_ref_active", cb_text);

    // set_relay_dut_active
    cb_text = [this](const String &text) {
        aux_hw_drv.set_relay_dut_active(text == "true");
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_relay_dut_active", cb_text);

    // set_fan_active
    cb_text = [this](const String &text) {
        aux_hw_drv.set_fan_active(text == "true");
        _send_state_changed_event();
    };
    api_server->register_api_cb("set_fan_active", cb_text);
}
