#ifndef APP_CONFIG_HPP__
#define APP_CONFIG_HPP__

#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

/** @brief Application configuration and default values for PsPwmAppHwControl
 */
struct PsPwmAppConfig
{
    ///////////////////////////// For ps_pwm C module: ////////////////////////
    // MCPWM unit can be [0,1]
    mcpwm_unit_t mcpwm_num{MCPWM_UNIT_0};
    // GPIO config for PWM output
    int gpio_pwm0a_out{27}; // PWM0A := LEAD leg, Low Side
    int gpio_pwm0b_out{26}; // PWM0B := LEAD leg, High Side
    int gpio_pwm1a_out{25}; // PWM1A := LAG leg, Low Side
    int gpio_pwm1b_out{33}; // PWM1B := LAG leg, High Side
    // Shutdown/fault input for PWM outputs
    int gpio_fault_shutdown{4};
    // Active low / active high selection for fault input pin
    mcpwm_fault_input_level_t fault_pin_active_level{MCPWM_LOW_LEVEL_TGR};
    // Define here if the output pins shall be forced low or high
    // or high-impedance when a fault condition is triggered.
    // PWMxA and PWMxB have the same type of action, see declaration in mcpwm.h
    mcpwm_action_on_pwmxa_t disable_action_lag_leg{MCPWM_FORCE_MCPWMXA_LOW};
    // Lead leg might have a different configuration, e.g. stay at last output level
    mcpwm_action_on_pwmxa_t disable_action_lead_leg{MCPWM_FORCE_MCPWMXA_LOW};

    // Default runtime frequency setpoint limits
    float frequency_min{50e3};
    float frequency_max{300e3};
    // Initial frequency setpoint
    float init_frequency{100e3};
    // Initial phase-shift setpoint
    float init_ps_duty{0.45};
    // Initial leading leg dead-time value
    float init_lead_dt{125e-9};
    // Initial lagging leg dead-time value
    float init_lag_dt{125e-9};
    // Initial output state should be "false" representing "off"
    bool init_power_pwm_active{false};

    /////////////////////////////  For AUX HW control module: /////////////////
    // Initial setpoints
    // ==> See aux_hw_drv.hpp <==
    
    /////////////////////////////  For API server /////////////////////////////
    /** Update non-critical application state and send cyclic
     * state updates to the HTTP client using this time interval (ms)
     */
    uint32_t api_state_periodic_update_interval_ms{200};
};


/** @brief Hardware configuration for AuxHwDrv
 */
struct AuxHwDrvConfig
{
    // GPIO config, outputs //
    static constexpr int gpio_fan{2};
    static constexpr int gpio_overcurrent_reset{16};
    static constexpr int gpio_relay_ref{18};
    static constexpr int gpio_relay_dut{19};
    static constexpr int gpio_delta_sigma_out{21};
    static constexpr int gpio_drv_supply_en{23};
    static constexpr int gpio_drv_disable{32};
    // Handled by LEDC PWM API
    static constexpr int gpio_curr_limit_reference_pwm{17};
    // GPIO config, inputs //
    static constexpr int gpio_delta_sigma_in{22};

    // Structures for GPIO and PWM API //
    static constexpr gpio_config_t aux_periph_gpio_output_config {
        .pin_bit_mask = ((1ULL<<gpio_fan)
                         |(1ULL<<gpio_overcurrent_reset)
                         |(1ULL<<gpio_relay_ref)
                         |(1ULL<<gpio_relay_dut)
                         |(1ULL<<gpio_delta_sigma_out)
                         |(1ULL<<gpio_drv_supply_en)
                         |(1ULL<<gpio_drv_disable)
                         ),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    static constexpr gpio_config_t aux_periph_gpio_input_config {
        .pin_bit_mask = ((1ULL<<gpio_delta_sigma_in)
                         ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    // PWM outputs config
    // Maximum PWM frequency for given resolution in N bits is:
    // freq_hz = 80 MHz / 2^N
    static constexpr ledc_timer_config_t pwm_timer_config {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 19500,
        .clk_cfg = LEDC_USE_APB_CLK // 80 MHz
    };
    static constexpr ledc_channel_config_t curr_lim_pwm_ch_config {
        .gpio_num = gpio_curr_limit_reference_pwm,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    // Same PWM timer is used for isolated external delta_sigma hardware pin
    static constexpr ledc_channel_config_t delta_sigma_out_pwm_ch_config {
        .gpio_num = gpio_delta_sigma_out,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    // Overcurrent reset output pulse length. Must be at least equal to
    // FreeRTOS scheduler tick period.
    static constexpr int oc_reset_pulse_length_ms{10};

    // Calibration values for current limit PWM
    static constexpr float curr_limit_pwm_scale = 1.0/100 * (
        1 << pwm_timer_config.duty_resolution);
    static constexpr uint32_t curr_limit_pwm_offset = 0;
    ///////////////////////////////////////////////////////// End Configuration
};

/***************** AuxHwDrv setpoints with initial values ******************//**
 * Used as public member of AuxHwDrv.
 * That instance is also accessed by PsPwmAppHwControl
 */
struct AuxHwDrvState
{
    float current_limit{8};
    bool relay_ref_active{false};
    bool relay_dut_active{false};
    float aux_temp{150};
    float heatsink_temp{150};
    bool fan_active{true};
    bool drv_supply_active{true};
    bool drv_disabled{false};
};


#endif