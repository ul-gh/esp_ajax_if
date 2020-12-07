/** @file app_hw_control.hpp
 *
 * License: GPL v.3 
 * U. Lukas 2020-09-27
 */
#ifndef APP_HW_CONTROL_HPP__
#define APP_HW_CONTROL_HPP__

#include <Ticker.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/timers.h"

#include <ArduinoJson.h>

#include "ps_pwm.h"
#include "aux_hw_drv.hpp"
#include "api_server.hpp"
#include "multi_timer.hpp"

#include "app_config.hpp"

// Only the up-counting mode using the hardware dead-band generator is safe
// for changing setpoints on-the-fly
#define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API


/** @brief Application state containing data and settings model
 * 
 * Live data is kept here and and can be serialised to be sent to the
 * connected remote clients.
 * 
 * Runtime user configurable settings can be serialised and stored to file
 * or read back from file and restored into this instance.
 */
struct PsPwmAppState
{
    // Configuration and initial values for the application state
    static constexpr AppConfig app_conf{};
    /** ATTENTION!
     * Following constants need to be adapted if JSON object size is changed!
     */
    static constexpr size_t _json_objects_size = JSON_OBJECT_SIZE(22);
    static constexpr size_t _strings_size = sizeof(
        "frequency_min_hw""frequency_max_hw""dt_sum_max_hw"
        "frequency_min""frequency_max"
        "frequency""duty""lead_dt""lag_dt""power_pwm_active"
        "current_limit""relay_ref_active""relay_dut_active"
        "aux_temp""heatsink_temp""fan_active"
        "base_div""timer_div"
        "drv_supply_active""drv_disabled"
        "hw_oc_fault_present""hw_oc_fault_occurred"
        );
    // Prevent buffer overflow even if above calculations are wrong...
    static constexpr size_t I_AM_SCARED_MARGIN = 50;
    static constexpr size_t json_size = _json_objects_size
                                        + _strings_size
                                        + I_AM_SCARED_MARGIN;

    /////////////////////// Runtime state starts here ///////////////////////
    // Zero-Copy values using pointers read from the PSPWM C API
    pspwm_clk_conf_t* pspwm_clk_conf = nullptr;
    pspwm_setpoint_t* pspwm_setpoint = nullptr;
    pspwm_setpoint_limits_t* pspwm_setpoint_limits = nullptr;
    // True when hardware OC shutdown condition is currently present
    bool hw_oc_fault_present = true;
    // Hardware Fault Shutdown Status is latched using this flag
    bool hw_oc_fault_occurred = true;
    // Runtime user settpoint limits for output frequency
    float frequency_min = app_conf.frequency_min;
    float frequency_max = app_conf.frequency_max;

    // State from AuxHwDrv module
    AuxHwDrvState *aux_hw_drv_state = nullptr;

    /** @brief Application live data state in JSON format.
     * 
     * You must call serialize_data() before using the content.
     */
    char json_buf_data[json_size];

    /** @brief Application runtime configurable settings in JSON format.
     * 
     * You must call serialize_settings() before using the content.
     */
    char json_buf_settings[json_size];

    /** @brief Serialize application live data into json_buf_data
     */
    void serialize_data();

    /** @brief Serialize application runtime configurable settings into json_buf
     */
    void serialize_settings();

    /** @brief Restore application runtime configurable settings
     * from json_buf_settings back into this instance.
     */
    void deserialize_settings();

    /** @brief Write application runtime configurable settings as JSON to SPIFFs file.
     */
    bool save_to_file(const char *filename);

    /** @brief Restore application runtime configurable settings
     * from SPIFFs file back into this instance.
     */
    bool restore_from_file(const char *filename);

};


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
    static constexpr AppConfig app_conf{};

    // Runtime state plus JSON serialisation import/export
    PsPwmAppState state;

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