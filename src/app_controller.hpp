/** @file app_controller.hpp
 *
 * License: GPL v.3 
 * U. Lukas 2020-12-11
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

// Only the up-counting mode using the hardware dead-band generator is safe
// for changing setpoints on-the-fly
#define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API


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
    void set_frequency_min_khz(float n);

    void set_frequency_max_khz(float n);

    void set_frequency_khz(float n);

    void set_duty_percent(float n);

    void set_lag_dt_ns(float n);

    void set_lead_dt_ns(float n);

    /* Activate PWM power output if arg is true
    */
    void set_power_pwm_active(bool state);

    void trigger_oneshot();

    void clear_shutdown();

    void set_current_limit(float n);

    void set_relay_ref_active(bool state);

    void set_relay_dut_active(bool state);

    void set_fan_active(bool state);


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
    MultiTimerNonStatic<AppController> power_output_timer;

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