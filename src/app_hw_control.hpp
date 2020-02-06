#ifndef APP_HW_CONTROL_HPP__
#define APP_HW_CONTROL_HPP__

#include "driver/mcpwm.h"
#include "ps_pwm.h"
#include "api_server.hpp"


/** PSPWMGen - Phase-Shift PWM output generation on ESP32 platform
 * 
 * By default, the symmetric-drive API is used.
 * This allows the setting of two individual dead-time values
 * for the leading and lagging driver output half-bridge-leg
 * and enforces a DC-free symmetric output waveform.
 * 
 * In order to allow four individual dead-time values, i.e. different
 * settings for both outputs of each half-bridge-leg, uncomment the
 * #define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API line.
 * 
 * Asymmetric drive allows twice the maximum output frequency by using
 * the hardware dead-time generator module but output is not guaranteed
 * DC-free.
 */
//#define USE_ASYMMETRIC_FULL_SPEED_DRIVE_API
#define USE_SYMMETRIC_DC_FREE_DRIVE_API
class PSPWMGen
{
public:
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

    // Initial state
    static constexpr float init_frequency{100e3};
    static constexpr float init_ps_duty{0.45};
    static constexpr float init_lead_dt{125e-9};
    static constexpr float init_lag_dt{125e-9};
    static constexpr bool init_output_enabled{false};

    // Message to send via HTTP Server-Sent-Events when HW shutdown occurs
    static constexpr const char* shutdown_message{"Hardware Shutdown occurred!"};
    // Normal reply
    static constexpr const char* normal_message{"OK"};

    pspwm_setpoint_t* pspwm_setpoint;
    pspwm_setpoint_limits_t* pspwm_setpoint_limits;

    PSPWMGen(APIServer &api_server);
    // virtual ~PSPWMGen();

    // Register hw control functions as request handlers with the HTPP server
    void register_remote_control(APIServer &api_server);

//private:
//    static void IRAM_ATTR fault_isr_handler(void* unused);
};

#endif