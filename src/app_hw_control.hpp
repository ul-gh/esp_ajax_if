/** @file app_hw_control.hpp
 *
 * License: GPL v.3 
 * U. Lukas 2020-09-27
 */
#ifndef APP_HW_CONTROL_HPP__
#define APP_HW_CONTROL_HPP__

//#include <Ticker.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"

#include <ArduinoJson.h>

#include "ps_pwm.h"
#include "aux_hw_drv.hpp"
#include "api_server.hpp"

// Only the up-counting mode using the hardware dead-band generator is safe
// for changing setpoints on-the-fly
#define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API

/** @brief Application configuration and default values
 */
struct PsPwmAppConfig
{
    ///////////////////////////// For ps_pwm C module: ////////////////////////
    // MCPWM unit can be [0,1]
    static constexpr mcpwm_unit_t mcpwm_num{MCPWM_UNIT_0};
    // GPIO config for PWM output
    static constexpr int gpio_pwm0a_out{27}; // PWM0A := LEAD leg, Low Side
    static constexpr int gpio_pwm0b_out{26}; // PWM0B := LEAD leg, High Side
    static constexpr int gpio_pwm1a_out{25}; // PWM1A := LAG leg, Low Side
    static constexpr int gpio_pwm1b_out{33}; // PWM1B := LAG leg, High Side
    // Shutdown/fault input for PWM outputs
    static constexpr int gpio_fault_shutdown{4};
    // Active low / active high selection for fault input pin
    static constexpr mcpwm_fault_input_level_t fault_pin_active_level{MCPWM_LOW_LEVEL_TGR};
    // Define here if the output pins shall be forced low or high
    // or high-impedance when a fault condition is triggered.
    // PWMxA and PWMxB have the same type of action, see declaration in mcpwm.h
    static constexpr mcpwm_action_on_pwmxa_t disable_action_lag_leg{MCPWM_FORCE_MCPWMXA_LOW};
    // Lead leg might have a different configuration, e.g. stay at last output level
    static constexpr mcpwm_action_on_pwmxa_t disable_action_lead_leg{MCPWM_FORCE_MCPWMXA_LOW};

    // Default runtime frequency setpoint limits
    static constexpr float frequency_min{50e3};
    static constexpr float frequency_max{300e3};
    // Initial frequency setpoint
    static constexpr float init_frequency{100e3};
    // Initial phase-shift setpoint
    static constexpr float init_ps_duty{0.45};
    // Initial leading leg dead-time value
    static constexpr float init_lead_dt{125e-9};
    // Initial lagging leg dead-time value
    static constexpr float init_lag_dt{125e-9};
    // Initial output state should be "false" representing "off"
    static constexpr bool init_power_pwm_active{false};

    /////////////////////////////  For AUX HW control module: /////////////////
    // Initial setpoints
    // ==> See aux_hw_drv.hpp <==
    
    /////////////////////////////  For API server /////////////////////////////
    /** Update non-critical application state and send cyclic
     * state updates to the HTTP client using this time interval (ms)
     */
    static constexpr uint32_t api_state_periodic_update_interval_ms{200};
};

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
    AuxHwDrv *aux_hw_drv = nullptr;

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
    // Event timer instance
    //Ticker periodic_update_timer;
    TimerHandle_t periodic_update_timer{NULL};

    // Called from this constructor
    void _initialize_ps_pwm_drv();

    /** Application state is sent as a push update via the SSE event source.
     *  See file: app_hw_control.cpp
     */
    // static void on_periodic_update_timer(PsPwmAppHwControl* self);
    static void on_periodic_update_timer(TimerHandle_t xTimer);
};

#endif