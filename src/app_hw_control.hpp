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

#include "app_config.hpp"

// Only the up-counting mode using the hardware dead-band generator is safe
// for changing setpoints on-the-fly
#define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API


/** @brief Application state
 * 
 * This is sent asynchronously on state changes to the connected browser clients
 * updating the view of the application state.
 * 
 * This is also stored on SPI flash for persistent configration.
 * 
 */
struct PsPwmAppState
{
    // Configuration and initial values for the application state
    static constexpr PsPwmAppConfig app_conf{};
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

    /** @brief Application state in JSON format. serialize() before.
     */
    char json_buf[json_size];

    /** @brief Serialize application state as a string in JSON format
     */
    void serialize();
};


/** @brief Application interface implementation for PS-PWM generator hardware
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
class PsPwmAppHwControl
{
public:
    // Configuration and initial values for the application state
    static constexpr PsPwmAppConfig app_conf{};

    // Runtime state plus JSON serialisation import/export
    PsPwmAppState state;

    // Instance of auxiliary HW control module
    AuxHwDrv aux_hw_drv;

    // Instance of HTTP API server. There must only be one.
    APIServer* api_server;

    PsPwmAppHwControl(APIServer* api_server);
    virtual ~PsPwmAppHwControl();

    /** Begin operation.
     * This also starts the timer callbacks etc.
     * This will fail if networking etc. is not set up correctly!
     */
    void begin();

    // Register hw control functions as request handlers with the HTPP server
    void register_remote_control(APIServer* api_server);

private:
    // Timer for periodic events
    Ticker periodic_update_timer;
    //TimerHandle_t periodic_update_timer{NULL};

    // Called from this constructor
    void _initialize_ps_pwm_drv();

    /** Application state is sent as a push update via the SSE event source.
     *  See file: app_hw_control.cpp
     */
    static void _on_periodic_update_timer(PsPwmAppHwControl *self);
    // Variant for FreeRTOS timer
    //static void _on_periodic_update_timer(TimerHandle_t xTimer);
};

#endif