#ifndef APP_CONFIG_HPP__
#define APP_CONFIG_HPP__

#include "FreeRTOS.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"

/** @brief Application configuration and default values for class AppController
 */
struct AppConfig
{
    ///////////////////////////// For AppController ///////////////////////////
    // App state serialization using the ArduionJSON module takes a lot of it
    uint32_t app_event_task_stack_size{4096};
    // Arduino loop task has 1; async_tcp task has 3.
    // Assuming 2 is a good choice in-between..
    UBaseType_t app_event_task_priority{2};
    // PRO_CPU_NUM == 1; APP_CPU_NUM == 0 on ESP32
    BaseType_t app_event_task_core_id{APP_CPU_NUM};
    // Fast timer for ADC conversion triggering etc
    uint32_t timer_fast_interval_ms{50};
    /** Update non-critical application state and send cyclic
     * state updates to the HTTP client using this time interval (ms)
     */
    uint32_t timer_slow_interval_ms{1000};

    ///////////////////////////// For ps_pwm C module: ////////////////////////
    // MCPWM unit can be [0,1]
    mcpwm_unit_t mcpwm_num{MCPWM_UNIT_0};
    // GPIO config for PWM output
    gpio_num_t gpio_pwm0a_out{GPIO_NUM_27}; // PWM0A := LEAD leg, Low Side
    gpio_num_t gpio_pwm0b_out{GPIO_NUM_26}; // PWM0B := LEAD leg, High Side
    gpio_num_t gpio_pwm1a_out{GPIO_NUM_25}; // PWM1A := LAG leg, Low Side
    gpio_num_t gpio_pwm1b_out{GPIO_NUM_33}; // PWM1B := LAG leg, High Side
    // Shutdown/fault input for PWM outputs
    gpio_num_t gpio_fault_shutdown{GPIO_NUM_4};
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
};


/** @brief ADC configuration and calibration data
 */
struct AdcTempConfig
{
    /************************************************************************
     * Configuration
     */

    /** @brief ADC channel for AUX temperature sensor */
    adc1_channel_t temp_ch_aux = ADC1_CHANNEL_0; // Sensor VP
    /** @brief ADC channel for heatsink temperature sensor */
    adc1_channel_t temp_ch_heatsink = ADC1_CHANNEL_3; // Sensor VN

    ////////// Configuration constants for get_kty_temp_lin()
    float temp_fsr_lower_lin = 0.0;
    float temp_fsr_upper_lin = 100.0;
    /** Voltages defining full-scale-range in mV */
    int32_t v_in_fsr_lower_lin = 886; // Corresponds to 0°C
    int32_t v_in_fsr_upper_lin = 1428; // Corresponds to 100°C
    ////////// Configuration constants for get_kty_temp_pwl()
    /** Voltages defining full-scale-range in mV */
    int32_t v_in_fsr_lower_lut = 596; // Corresponds to -55°C
    int32_t v_in_fsr_upper_lut = 1646; // Corresponds to 150°C

    /** @brief Look-Up-Table temperatures for 31 equidistant voltage steps.
     * Table only valid for linearised circuit using 2.2 kOhms series resistor
     * where ADC input voltage steps correspond to the following
     * temperature values in °C.
     * 
     * For LUT values, see ../util/kty81_1xx_sensor_generate_lut/kty81_lut.py
     */
    std::array<const float, 32> lut_temp {
    // For KTY81-121:
        -55.0       , -48.22273805, -41.51141124, -34.84623091,
        -28.34434926, -22.05459193, -15.78849403,  -9.53746745,
         -3.3772341 ,   2.7675195 ,   8.9372679 ,  15.0916243 ,
         21.14820431,  27.2082161 ,  33.34543424,  39.41134763,
         45.57173941,  51.73398583,  57.85244115,  64.10680179,
         70.45422093,  76.763773  ,  83.14712256,  89.64071316,
         96.17984636, 102.82297981, 109.58309561, 116.4296579 ,
        123.60532846, 131.27866698, 139.78106609, 150.0};
    // For KTY81-110 and KTY81-120:
    //std::array<const float, 32> lut_temp {
    //    -55.0       , -48.16279303, -41.39749472, -34.8911357 ,
    //    -28.54294667, -22.192432  , -15.83544756,  -9.56004681,
    //     -3.43833483,   2.66313257,   8.80135444,  14.90432723,
    //     20.97767882,  27.03976174,  33.13792626,  39.28966437,
    //     45.38382931,  51.48407173,  57.67841773,  63.97159787,
    //     70.30279723,  76.61562129,  83.00362829,  89.50586837,
    //     96.07234208, 102.68301035, 109.39886725, 116.34253305,
    //    123.5137051 , 131.2558412 , 139.76912438, 150.0};

    ////////// ADC hardware initialisation constants
    uint32_t default_vref{1100};
    uint16_t oversampling_ratio{64};
    uint16_t moving_average_filter_len{16};
    adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;

    /** @brief Suggested ADC input voltage Range for ESP32 using ADC_ATTEN_DB_6
     * is 150 ~ 1750 millivolts according to the SDK documentation for function
     * adc1_config_channel_atten(). With reduced accuracy, FSR is approx. 2.2V.
     */
    adc_atten_t temp_sense_attenuation = ADC_ATTEN_DB_6;
    adc_unit_t unit = ADC_UNIT_1;
};


/** @brief Hardware configuration for AuxHwDrv
 */
struct AuxHwDrvConfig
{
    // GPIO config, outputs //
    static constexpr gpio_num_t gpio_fan{GPIO_NUM_2};
    static constexpr gpio_num_t gpio_overcurrent_reset{GPIO_NUM_16};
    static constexpr gpio_num_t gpio_relay_ref{GPIO_NUM_18};
    static constexpr gpio_num_t gpio_relay_dut{GPIO_NUM_19};
    static constexpr gpio_num_t gpio_delta_sigma_out{GPIO_NUM_21};
    static constexpr gpio_num_t gpio_drv_supply_en{GPIO_NUM_23};
    static constexpr gpio_num_t gpio_drv_disable{GPIO_NUM_32};
    // Handled by LEDC PWM API
    static constexpr gpio_num_t gpio_curr_limit_reference_pwm{GPIO_NUM_17};
    // GPIO config, inputs //
    static constexpr gpio_num_t gpio_delta_sigma_in{GPIO_NUM_22};

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
    static constexpr uint32_t oc_reset_pulse_length_ms{20};

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