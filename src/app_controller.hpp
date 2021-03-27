/** @file app_controller.hpp
 * @brief Application Controller Implementing C++ and HTTP GET API
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
 * U. Lukas 2020-12-07
 */
#ifndef APP_CONTROLLER_HPP__
#define APP_CONTROLLER_HPP__

#include <Ticker.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/timers.h"

#include "aux_hw_drv.hpp"
#include "api_server.hpp"
#include "multi_timer.hpp"

#include "app_config.hpp"
#include "app_state_model.hpp"


/** @brief Application main controller for PS-PWM generator hardware
 *
 * This features the main control functions for PWM frequency, duty cycle etc.
 * 
 * Also, periodic state feedback for all hardware functions is sent to the
 * HTTP remote interface using Server-Sent Events from a FreeRTOS timer task.
 * 
 * Some auxiliary functions like GPIO and temperature readouts is outsourced
 * to the AuxHwDrv class, see aux_hw_drv.hpp.
 * 
 * In more detail:
 * This configures all parameters of a four-channel Phase-Shift PWM waveform
 * plus auxiliary hardware setpoints, relay outputs etc.
 * 
 */

class AppController
{
public:
    /** @brief Configuration and initial values for the application state
     */
    static constexpr auto app_conf = AppConfig{};

    // Runtime state plus JSON serialisation import/export
    AppState state;

    // Instance of auxiliary HW control module
    AuxHwDrv aux_hw_drv;

    // Instance of HTTP API server. There must only be one.
    APIServer* api_server;

    AppController(APIServer* api_server);
    virtual ~AppController();

    /** @brief Begin operation.
     * This also starts the timer callbacks etc.
     * This will fail if networking etc. is not set up correctly!
     */
    void begin();

    //////////// Application API ///////////
    void set_setpoint_throttling_enabled(bool new_val);

    void set_frequency_min_khz(float n);
    void set_frequency_max_khz(float n);
    void set_frequency_khz(float n);
    /** @brief Set rate of change of frequency in kHz per second
     */
    void set_frequency_changerate_khz_sec(float n);
    void _set_frequency_raw(float n);

    void set_duty_min_percent(float n);
    void set_duty_max_percent(float n);
    void set_duty_percent(float n);
    /** @brief Set rate of change of duty cycle in percent per second
     */
    void set_duty_changerate_percent_sec(float n);
    void _set_duty_raw(float n);

    void set_lag_dt_ns(float n);
    void set_lead_dt_ns(float n);

    /** @brief Activate PWM power output if arg is true
     */
    void set_power_pwm_active(bool new_val);

    /** @brief Set power output oneshot pulse timer pulse length in seconds
     */
    void set_oneshot_len(float n);

    /** @brief Trigger the power output oneshot pulse
     */
    void trigger_oneshot();

    void clear_shutdown();

    void set_current_limit(float n);

    /** @brief Set overtemperature shutdown limits
     */
    void set_temp_1_limit(float n);
    void set_temp_2_limit(float n);

    void set_relay_ref_active(bool new_val);

    void set_relay_dut_active(bool new_val);

    /** @brief When set to true, override automatic and set fan permanently on
     */
    void set_fan_override(bool new_val);

    /** @brief Save all runtime configurable settings to SPI flash.
     * The settings are a subset of all values in struct AppState.
     * 
     * The stored settings are restored on reboot.
     */
    void save_settings();

    /** @brief Read state back from SPI flash file and initialize the hardware
     * with these settings.
     * 
     * This is called on boot.
     */
    void restore_settings();


private:
    // FreeRTOS task handle for application event task
    static TaskHandle_t _app_event_task_handle;
    // FreeRTOS event group handle for triggering event task actions
    static EventGroupHandle_t _app_event_group;
    // Timer for periodic events
    Ticker event_timer_fast;
    Ticker event_timer_slow;
    // Timer for generating overcurrent reset pulse
    MultiTimer oc_reset_timer;
    // Timer for power output timing
    MultiTimer power_output_timer;

    /////////// Setup functions called from this constructor //////
    
    void _initialize_ps_pwm_drv();
    /** @brief Creates main application event task.
     * This has 4096k stack size for String processing requirements etc.
     */
    void _create_app_event_task();

    /** @brief Register all application HTTP GET API callbacks into the HTPP server
     */
    void _register_http_api(APIServer* api_server);

    /** @brief Connect timer callbacks
     */
    void _connect_timer_callbacks();

    //////////// Application task related functions ///////////
    
    /** @brief Application event loop task.
     */
    static void _app_event_task(void *pVParameters);

    /** @brief Update all application state settings which need fast polling.
     * This is e.g. ADC conversion and HW overcurrent detection handling
     */
    void _on_fast_timer_event_update_state();

    /** @brief Perform overtemperature shutdown if temperature limit exceeded
     */
    void _evaluate_temperature_sensors();

    /** @brief Called when app state is changed and triggers the respective event.
     * Used for sending push updates to the clients.
     */
    static void _send_state_changed_event();

    /** @brief Application state is sent as a push update via the SSE event source.
     *  See file: app_hw_control.cpp
     */
    void _push_state_update();
};

#endif