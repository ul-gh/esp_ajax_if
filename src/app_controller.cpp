/* Application Controller Implementing C++ and HTTP GET API
 *
 * This features the main control functions for PWM frequency, duty cycle etc.
 * 
 * Also, periodic state feedback for all hardware functions is sent to the
 * HTTP remote application using Server-Sent Events from a FreeRTOS timer task.
 * 
 * Some auxiliary functions like GPIO and temperature readouts is outsourced
 * to the AuxHwDrv class, see aux_hw_drv.cpp.
 * 
 * License: GPL v.3 
 * U. Lukas 2021-01-21
 */
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include <Arduino.h>

#include "ps_pwm.h"
#include "app_controller.hpp"

#undef LOG_LOCAL_LEVEL
// When setting log level to ESP_LOG_DEBUG:
// AppConfig::timer_fast_interval_ms must be at least 50
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
static auto TAG = "AppController";

/* Perform setpoint rate throttling, see definition at bottom of this file
 */
bool throttle_value(float *x_current, float x_target, float x_increment);

/** FreeRTOS event group bits definition for application event_task event loop
 */
struct EventFlags
{
    enum {TIMER_FAST, TIMER_SLOW, STATE_CHANGED, CONFIG_CHANGED};
    static constexpr EventBits_t timer_fast{1<<TIMER_FAST};
    static constexpr EventBits_t timer_slow{1<<TIMER_SLOW};
    static constexpr EventBits_t state_changed{1<<STATE_CHANGED};
    static constexpr EventBits_t config_changed{1<<CONFIG_CHANGED};

    const EventBits_t value;

    EventFlags(EventBits_t event_bitset)
        : value{event_bitset}
    {}

    /** Returns true if event flags are set at input bitmask position.
     * Bitmask values are defined in this struct.
     */
    bool have(EventBits_t bitmask) const {
        return (value & bitmask) == bitmask;
    }
};


// FreeRTOS task handle for application event task
TaskHandle_t AppController::_app_event_task_handle;
// FreeRTOS event group handle for triggering event task actions
EventGroupHandle_t AppController::_app_event_group;

AppController::AppController(APIServer *api_server)
    // HTTP AJAX API server instance was created before
    : api_server{api_server}
{
    assert(api_server);
    // Reads this.app_conf and sets this.state
    _initialize_ps_pwm_drv();
    state.aux_hw_drv_state = &aux_hw_drv.state;
    _create_app_event_task();
}

AppController::~AppController() {
    event_timer_fast.detach();
    event_timer_slow.detach();
    oc_reset_timer.detach();
    power_output_timer.detach();
}


/* Begin operation.
 * This also starts the timer callbacks etc.
 * This will fail if networking etc. is not set up correctly!
 */
void AppController::begin() {
    restore_settings();
    _connect_timer_callbacks();
    _register_http_api(api_server);
}


//////////// Application API ///////////
void AppController::set_setpoint_throttling_enabled(bool new_val) {
    state.setpoint_throttling_enabled = new_val;
    _send_state_changed_event();
}

void AppController::set_frequency_min_khz(float n) {
    state.frequency_min = n * 1E3;
    _send_state_changed_event();
}
void AppController::set_frequency_max_khz(float n) {
    state.frequency_max = n * 1E3;
    _send_state_changed_event();
}

void AppController::set_frequency_khz(float n) {
    auto f_requested = std::max(n*1e3f, state.frequency_min);
    state.frequency_target = std::min(f_requested, state.frequency_max);
    if (!state.setpoint_throttling_enabled) {
        _set_frequency_raw(state.frequency_target);
    }
}
void AppController::set_frequency_changerate_khz_sec(float n) {
    state.frequency_increment = n * app_conf.timer_fast_interval_ms;
}
void AppController::_set_frequency_raw(float n) {
    // state.pspwm_setpoint->frequency = n;
    pspwm_set_frequency(app_conf.mcpwm_num, n);
    _send_state_changed_event();
}

void AppController::set_duty_percent(float n) {
    state.duty_target = n*0.01f;
    if (!state.setpoint_throttling_enabled) {
        _set_duty_raw(state.duty_target);
    }
}
void AppController::set_duty_changerate_percent_sec(float n) {
    state.duty_increment = n * app_conf.timer_fast_interval_ms * 1e-5f;
    _send_state_changed_event();
}
void AppController::_set_duty_raw(float n) {
    // state.pspwm_setpoint->ps_duty = n;
    pspwm_set_ps_duty(app_conf.mcpwm_num, n);
    _send_state_changed_event();
}

void AppController::set_lag_dt_ns(float n) {
    state.pspwm_setpoint->lag_red = n * 1e-9;
    pspwm_set_deadtimes_symmetrical(app_conf.mcpwm_num,
                                    state.pspwm_setpoint->lead_red,
                                    state.pspwm_setpoint->lag_red);
    _send_state_changed_event();
}
void AppController::set_lead_dt_ns(float n) {
    state.pspwm_setpoint->lead_red = n * 1e-9;
    pspwm_set_deadtimes_symmetrical(app_conf.mcpwm_num,
                                    state.pspwm_setpoint->lead_red,
                                    state.pspwm_setpoint->lag_red);
    _send_state_changed_event();
}

/* Activate PWM power output if arg is true
 */
void AppController::set_power_pwm_active(bool new_val) {
    if (new_val == true) {
        // For the tate.hw_oc_fault_xxx flags, these are
        // automatically checked by pspwm module on reactivation
        if (aux_hw_drv.state.hw_overtemp) {
            ESP_LOGE(TAG, "Overtemperature shutdown still active!");
            return;
        }
        if (state.setpoint_throttling_enabled) {
            // Begin with duty = 0.0 for soft start
            // state.pspwm_setpoint->ps_duty = n;
            pspwm_set_ps_duty(app_conf.mcpwm_num, 0.0f);
        }
        // aux_hw_drv.set_drv_disabled(false);
        pspwm_resync_enable_output(app_conf.mcpwm_num);
    } else {
        // aux_hw_drv.set_drv_disabled(true);
        pspwm_disable_output(app_conf.mcpwm_num);
    }
    _send_state_changed_event();
}

/* Set power output oneshot pulse timer pulse length in seconds
 */
void AppController::set_oneshot_len(float n) {
    state.oneshot_power_pulse_length_ms = static_cast<uint32_t>(n * 1000);
    _send_state_changed_event();
}

/* Trigger the power output oneshot pulse
 */
void AppController::trigger_oneshot() {
    // The timer callback also sends a state_changed event
    power_output_timer.start(state.oneshot_power_pulse_length_ms);
}

// The output is /not/ enabled again, it must be re-enabled explicitly.
void AppController::clear_shutdown() {
    aux_hw_drv.state.hw_overtemp = false;
    // This sets the aux_hw_drv.state.hw_overtemp flag back again should
    // the temperature still be above limits.
    aux_hw_drv.evaluate_temperature_sensors();
    if (state.hw_oc_fault_occurred) {
        // The timer callback generates a three-cycle reset pulse and
        // sends a state_changed event when finished.
        oc_reset_timer.start();
    } else {
        _send_state_changed_event();
    }
}

void AppController::set_current_limit(float n) {
    aux_hw_drv.set_current_limit(n);
    _send_state_changed_event();
}

void AppController::set_temp_1_limit(float n) {
    aux_hw_drv.state.temp_1_limit = n;
    _send_state_changed_event();
}

void AppController::set_temp_2_limit(float n) {
    aux_hw_drv.state.temp_2_limit = n;
    _send_state_changed_event();
}

void AppController::set_relay_ref_active(bool new_val) {
    aux_hw_drv.set_relay_ref_active(new_val);
    _send_state_changed_event();
}
void AppController::set_relay_dut_active(bool new_val) {
    aux_hw_drv.set_relay_dut_active(new_val);
    _send_state_changed_event();
}
void AppController::set_fan_override(bool new_val) {
    aux_hw_drv.set_fan_override(new_val);
    _send_state_changed_event();
}


/* Save all runtime configurable settings to SPI flash.
 * The settings are a subset of all values in struct AppState.
 * 
 * The stored settings are restored on reboot.
 */
void AppController::save_settings() {
    state.save_to_file(app_conf.settings_filename);
    _send_state_changed_event();
}

/* Read state back from SPI flash file and initialize the hardware
 * with these settings.
 * 
 * Called on boot when the application task event loop is not yet running.
 */
void AppController::restore_settings() {
    ESP_LOGI(TAG, "Restoring state from settings.json...");
    state.restore_from_file(app_conf.settings_filename);
    // We only need to run the setters for properties which affect the hardware.
    set_frequency_khz(state.frequency_target * 1E-3f);
    set_duty_percent(state.duty_target * 100.0f);
    set_lead_dt_ns(state.pspwm_setpoint->lead_red * 1E9f);
    set_lag_dt_ns(state.pspwm_setpoint->lag_red * 1E9f);
    set_current_limit(state.aux_hw_drv_state->current_limit);
    set_temp_1_limit(state.aux_hw_drv_state->temp_1_limit);
    set_temp_2_limit(state.aux_hw_drv_state->temp_2_limit);
    set_relay_ref_active(state.aux_hw_drv_state->relay_ref_active);
    set_relay_dut_active(state.aux_hw_drv_state->relay_dut_active);
    set_fan_override(state.aux_hw_drv_state->fan_override);
    aux_hw_drv.update_temperature_sensors();
    _evaluate_temperature_sensors();
    // There is no API for this at the moment, so this is always active..
    ESP_LOGI(TAG, "Activating Gate driver power supply...");
    aux_hw_drv.set_drv_supply_active(true);
}

/////////// Setup functions called from this constructor //////

void AppController::_initialize_ps_pwm_drv() {
    ESP_LOGI(TAG, "Configuring Phase-Shift-PWM...");
    auto errors = pspwm_init_symmetrical(app_conf.mcpwm_num,
                                         app_conf.gpio_pwm0a_out,
                                         app_conf.gpio_pwm0b_out,
                                         app_conf.gpio_pwm1a_out,
                                         app_conf.gpio_pwm1b_out,
                                         app_conf.init_frequency,
                                         app_conf.init_ps_duty,
                                         app_conf.init_lead_dt,
                                         app_conf.init_lag_dt,
                                         app_conf.init_power_pwm_active,
                                         app_conf.disable_action_lead_leg,
                                         app_conf.disable_action_lag_leg);
    errors |= pspwm_get_setpoint_limits_ptr(app_conf.mcpwm_num,
                                            &state.pspwm_setpoint_limits);
    errors |= pspwm_get_setpoint_ptr(app_conf.mcpwm_num,
                                     &state.pspwm_setpoint);
    errors |= pspwm_get_clk_conf_ptr(app_conf.mcpwm_num,
                                     &state.pspwm_clk_conf);
    errors |= pspwm_enable_hw_fault_shutdown(app_conf.mcpwm_num,
                                             app_conf.gpio_fault_shutdown,
                                             MCPWM_LOW_LEVEL_TGR);
    // Pull-down enabled for low-level shutdown active default state
    errors |= gpio_pulldown_en(app_conf.gpio_fault_shutdown);
    if (errors != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing the PS-PWM module!");
        abort();
    }
}

void AppController::_create_app_event_task() {
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


/* Register all application HTTP GET API callbacks into the HTPP server
 */
void AppController::_register_http_api(APIServer* api_server) {
    CbVoidT cb_void;
    CbFloatT cb_float;
    CbStringT cb_text;
    // "set_setpoint_throttling_enabled"
    cb_text = [this](const String &text) {set_setpoint_throttling_enabled(text=="true");};
    api_server->register_api_cb("set_setpoint_throttling_enabled", cb_text);
    // "set_frequency_min"
    cb_float = [this](float n) {set_frequency_min_khz(n);};
    api_server->register_api_cb("set_frequency_min", cb_float);
    // "set_frequency_max"
    cb_float = [this](float n) {set_frequency_max_khz(n);};
    api_server->register_api_cb("set_frequency_max", cb_float);
    // "set_frequency"
    cb_float = [this](float n) {set_frequency_khz(n);};
    api_server->register_api_cb("set_frequency", cb_float);
    // "set_frequency_changerate"
    cb_float = [this](float n) {set_frequency_changerate_khz_sec(n);};
    api_server->register_api_cb("set_frequency_changerate", cb_float);
    // "set_duty"
    cb_float = [this](float n) {set_duty_percent(n);};
    api_server->register_api_cb("set_duty", cb_float);
    // "set_duty_changerate"
    cb_float = [this](float n) {set_duty_changerate_percent_sec(n);};
    api_server->register_api_cb("set_duty_changerate", cb_float);
    // "set_lag_dt"
    cb_float = [this](float n) {set_lag_dt_ns(n);};
    api_server->register_api_cb("set_lag_dt", cb_float);
    // "set_lead_dt"
    cb_float = [this](float n) {set_lead_dt_ns(n);};
    api_server->register_api_cb("set_lead_dt", cb_float);
    // "set_power_pwm_active"
    cb_text = [this](const String &text) {set_power_pwm_active(text=="true");};
    api_server->register_api_cb("set_power_pwm_active", cb_text);
    // "set_oneshot_len"
    cb_float = [this](float n){set_oneshot_len(n);};
    api_server->register_api_cb("set_oneshot_len", cb_float);
    // "trigger_oneshot"
    cb_void = [this](){trigger_oneshot();};
    api_server->register_api_cb("trigger_oneshot", cb_void);
    // "clear_shutdown"
    cb_void = [this](){clear_shutdown();};
    api_server->register_api_cb("clear_shutdown", cb_void);
    // "set_current_limit"
    cb_float = [this](float n) {set_current_limit(n);};
    api_server->register_api_cb("set_current_limit", cb_float);
    // "set_temp_1_limit"
    cb_float = [this](float n) {set_temp_1_limit(n);};
    api_server->register_api_cb("set_temp_1_limit", cb_float);
    // "set_temp_2_limit"
    cb_float = [this](float n) {set_temp_2_limit(n);};
    api_server->register_api_cb("set_temp_2_limit", cb_float);
    // "set_relay_ref_active"
    cb_text = [this](const String &text) {set_relay_ref_active(text=="true");};
    api_server->register_api_cb("set_relay_ref_active", cb_text);
    // "set_relay_dut_active"
    cb_text = [this](const String &text) {set_relay_dut_active(text=="true");};
    api_server->register_api_cb("set_relay_dut_active", cb_text);
    // "set_fan_override"
    cb_text = [this](const String &text) {set_fan_override(text=="true");};
    api_server->register_api_cb("set_fan_override", cb_text);
    // "save_settings"
    cb_void = [this](){save_settings();};
    api_server->register_api_cb("save_settings", cb_void);
}

/* Connect timer callbacks. These are run from esp_timer task.
 */
void AppController::_connect_timer_callbacks(){
    esp_err_t errors;
    // Configure timers triggering periodic events.
    // Fast events are used for triggering ADC conversion etc.
    event_timer_fast.attach_ms(
        app_conf.timer_fast_interval_ms,
        [](){xEventGroupSetBits(_app_event_group, EventFlags::timer_fast);}
        );
    // Slow events are used for sending periodic SSE push messages updating the
    // application state as displayed by the remote clients
    event_timer_slow.attach_ms(
        app_conf.timer_slow_interval_ms,
        [](){xEventGroupSetBits(_app_event_group, EventFlags::timer_slow);}
        );
    // Timer for generating output pulses
    errors = power_output_timer.attach_static_ms(
        state.oneshot_power_pulse_length_ms,
        2,
        [](AppController *self, uint32_t repeat_count){
            ESP_LOGD(TAG, "Power pulse callback called. Counter: %d    ms: %lu",
                     repeat_count, millis());
            if (repeat_count == 1) {
                self->set_power_pwm_active(true);
            } else {
                // Output was activated before. Disable it again.
                self->set_power_pwm_active(false);
            }
            self->_send_state_changed_event();
        }, 
        this,
        true
        );
    // Hardware overcurrent reset needs a pulse which is generated by this timer
    //
    // FIXME: Hardware has redundant latch but no separate oc detect line.
    //        So this currently does not recognize if error is still
    //        present or only latched.
    //if (pspwm_get_hw_fault_shutdown_present(mcpwm_num)) {
    //    ESP_LOGE(TAG, "Will Not Clear: Fault Shutdown Pin Still Active!");
    //    return;
    //}
    //
    // This multitimer instance calls the lambda three times in a row.
    // First call sets the hardware reset line active. Second call resets it.
    // Third call resets the PSPWM module internal error flag and sends a
    // notification event to the application.
    // The power output is /not/ enabled again, it must be re-enabled explicitly.
    errors |= oc_reset_timer.attach_static_ms(
        aux_hw_drv.aux_hw_conf.oc_reset_pulse_length_ms,
        3,
        [](AppController* self, uint32_t repeat_count){
            ESP_LOGD(TAG, "Reset called. Counter: %d    ms: %lu",
                     repeat_count, millis());
            if (repeat_count == 1) {
                self->aux_hw_drv.reset_oc_shutdown_start();
            } else if (repeat_count == 2) {
                self->aux_hw_drv.reset_oc_shutdown_finish();
            } else if (repeat_count == 3) {
                ESP_LOGD(TAG, "External HW reset done. Resetting SOC fault latch...");
                pspwm_clear_hw_fault_shutdown_occurred(self->app_conf.mcpwm_num);
                self->_send_state_changed_event();
            }
        },
        this,
        true
        );
    // Timers are essential
    if (errors != ESP_OK) {
        ESP_LOGE(TAG, "Application timer initialization failed! Abort..");
        abort();
    }
}

//////////// Application task related functions ///////////

/* AppHwControl application event task
 */
void AppController::_app_event_task(void *pVParameters) {
    auto self = static_cast<AppController*>(pVParameters);
    ESP_LOGI(TAG, "Starting AppController event task");
    // Main application event loop
    while (true) {
        const auto flags = EventFlags{
            xEventGroupWaitBits(
                _app_event_group, 0xFF, pdTRUE, pdFALSE, portMAX_DELAY)};
        if (flags.have(EventFlags::timer_fast)) {
            self->_on_fast_timer_event_update_state();
        }
        if (flags.have(EventFlags::timer_slow)) {
            self->_evaluate_temperature_sensors();
            self->_push_state_update();
        }
        if (flags.have(EventFlags::state_changed)) {
            self->_push_state_update();
        }
    }
}

/* Update all application state settings which need fast polling.
 * This is e.g. ADC conversion and HW overcurrent detection handling
 */
void AppController::_on_fast_timer_event_update_state() {
    // True when hardware OC shutdown condition is present
    state.hw_oc_fault_present = pspwm_get_hw_fault_shutdown_present(app_conf.mcpwm_num);
    // Hardware Fault Shutdown Status is latched using this flag
    state.hw_oc_fault_occurred = pspwm_get_hw_fault_shutdown_occurred(app_conf.mcpwm_num);
    // Update temperature sensor values on this occasion.
    // With averaging of 64 samples, both channels acquisition
    // takes approx. 9 ms combined.
    aux_hw_drv.update_temperature_sensors();
    // Apply setpoint throttling
    if (state.setpoint_throttling_enabled) {
        auto values_differ = throttle_value(&state.pspwm_setpoint->ps_duty,
                                            state.duty_target,
                                            state.duty_increment);
        if (values_differ) {
            _set_duty_raw(state.pspwm_setpoint->ps_duty);
        }
        values_differ = throttle_value(&state.pspwm_setpoint->frequency,
                                       state.frequency_target,
                                       state.frequency_increment);
        if (values_differ) {
            _set_frequency_raw(state.pspwm_setpoint->frequency);
        }
    }
}

/* Perform overtemperature shutdown if temperature limit exceeded
 */
void AppController::_evaluate_temperature_sensors() {
    aux_hw_drv.evaluate_temperature_sensors();
    if (aux_hw_drv.state.hw_overtemp) {
        // aux_hw_drv.set_drv_disabled(true);
        pspwm_disable_output(app_conf.mcpwm_num);
        // State update is automatically pushed from slow timer loop
    }
}

/* Called when app state is changed and triggers the respective event.
 * Used for sending push updates to the clients.
 */
void AppController::_send_state_changed_event() {
    xEventGroupSetBits(_app_event_group, EventFlags::state_changed);
}

/* Send SSE push update to all connected clients.
 * 
 * Called periodicly (default once per second) but also asynchronously
 * on demand when state_change event is received.
 */
void AppController::_push_state_update() {
    assert(api_server && api_server->event_source);
    constexpr auto len = AppState::json_buf_len;
    auto json_buf = std::array<char, len>{};
    state.serialize_full_state(json_buf.data(), len);
    api_server->event_source->send(json_buf.data(), "hw_app_state");
}

/* Perform setpoint change rate throttling to a value at ptr x_current
 * by adding or subtracting a maximum x_increment for each invocation
 * of this function until the final value x_target is reached.
 * This does float equality evaluation w/o epsilon which should be safe here
 * as it is done only after adding/subtracting an exact float type difference.
 */
bool throttle_value(float *x_current, float x_target, float x_increment) {
    auto dx = x_target - *x_current;
    if (dx == 0.0f) {
        return false;
    }
    ESP_LOGD(TAG, "Throttling. Value is: %f.  Target: %f.  Increment: %f.",
                  *x_current, x_target, x_increment);
    if (dx > 0.0f) {
        *x_current += std::min(dx, x_increment);
    } else { // (dx < 0.0f)
        *x_current += std::max(dx, -x_increment);
    }
    return true;
}