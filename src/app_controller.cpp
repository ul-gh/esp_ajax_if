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
 * U. Lukas 2020-12-07
 */
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/mcpwm.h"
#include <Arduino.h>

#include "ps_pwm.h"
#include "app_controller.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "app_controller.cpp";

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

AppController::AppController(APIServer* api_server)
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

AppController::~AppController() {
    event_timer_fast.detach();
    event_timer_slow.detach();
    oc_reset_timer.detach();
    power_output_timer.detach();
}

/** Begin operation.
 * This also starts the timer callbacks etc.
 * This will fail if networking etc. is not set up correctly!
 */
void AppController::begin() {
    ESP_LOGD(TAG, "Activating Gate driver power supply...");
    aux_hw_drv.set_drv_supply_active("true");
    _register_http_api(api_server);
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
}

/////////// Setup functions called from this constructor //////

void AppController::_initialize_ps_pwm_drv() {
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
        //        So this currently does not recognize if error is still
        //        present or only latched.
        //if (pspwm_get_hw_fault_shutdown_present(mcpwm_num)) {
        //    ESP_LOGE(TAG, "Will Not Clear: Fault Shutdown Pin Still Active!");
        //    return;
        //}
        //
        // Set the GPIO pin to reset external fault latch
        aux_hw_drv.reset_oc_shutdown_start();
        // This multitimer instance calls the lambda two times in a row.
        // First call sets the external reset pin inactive, second call clears
        // the SoC internal fault latch in the MCPWM hardware stage.
        oc_reset_timer.attach_multitimer_ms(
            aux_hw_drv.aux_hw_conf.oc_reset_pulse_length_ms,
            2,
            [](AppController* self, uint32_t repeat_count){
                ESP_LOGD(TAG, "Reset called... Counter is: %d  millis is: %lu",
                         repeat_count, millis());
                if (repeat_count == 1) {
                    self->aux_hw_drv.reset_oc_shutdown_finish();
                } else if (repeat_count == 2) {
                    ESP_LOGD(TAG, "External HW reset done. Resetting SOC fault latch...");
                    pspwm_clear_hw_fault_shutdown_occurred(self->app_conf.mcpwm_num);
                    self->_send_state_changed_event();
                }
            },
            this);
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

//////////// Application task related functions ///////////

/** AppHwControl application event task
 */
void AppController::_app_event_task(void *pVParameters) {
    auto self = static_cast<AppController*>(pVParameters);
    ESP_LOGD(TAG, "Starting AppController event task");
    // Main application event loop
    while (true) {
        const EventFlags flags = xEventGroupWaitBits(
            _app_event_group, 0xFF, pdTRUE, pdFALSE, portMAX_DELAY);
        if (flags.have(EventFlags::timer_fast)) {
            self->_on_fast_timer_event_update_state();
        }
        if (flags.have(EventFlags::timer_slow)) {
            self->_push_state_update();
        }
        if (flags.have(EventFlags::state_changed)) {
            self->_push_state_update();
        }
    }
}

/** Update all application state settings which need fast polling.
 * This is e.g. ADC conversion and HW overcurrent detection handling
 */
void AppController::_on_fast_timer_event_update_state() {
    // True when hardware OC shutdown condition is present
    state.hw_oc_fault_present = pspwm_get_hw_fault_shutdown_present(app_conf.mcpwm_num);
    // Hardware Fault Shutdown Status is latched using this flag
    state.hw_oc_fault_occurred = pspwm_get_hw_fault_shutdown_occurred(app_conf.mcpwm_num);
    // Update temperature sensor values on this occasion
    aux_hw_drv.update_temperature_sensors();
}

/** Application state is sent as a push update via the SSE event source.
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
    // Prepare JSON message for sending
    state.serialize_data();
    api_server->event_source->send(state.json_buf_data, "hw_app_state");
}
