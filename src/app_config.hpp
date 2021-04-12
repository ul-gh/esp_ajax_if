#ifndef APP_CONFIG_HPP__
#define APP_CONFIG_HPP__

#include "FreeRTOS.h"
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"

#include "IPAddress.h"
#include "ArduinoJson.h"

constexpr auto serial_baudrate = 115200ul;


/** @brief WiFi network configuration structure with default values
 * 
 * These are to be overridden with user-set values stored on NVS.
 * 
 * There is NO secure boot / flash encryption activated currently.
 * Do not share password for different services or purposes!
 */
struct NetworkConfig {
    ///////// Constants /////////
    static constexpr size_t ssid_maxlen = 32 + 1;
    static constexpr size_t psk_maxlen = 63 + 1;
    static constexpr size_t hostname_maxlen = 32 + 1;

    static constexpr const char* get_wifi_config_endpoint = "/get_wifi_config";
    static constexpr const char* set_wifi_config_endpoint = "/set_wifi_config";
    static constexpr uint16_t http_tcp_port = 80;

    // Maximum number of device reboots when multiple reconnections have failed
    static constexpr uint32_t max_reboots = 5;
    // Maximum number of connection attempts for configured acces point in station mode
    static constexpr uint32_t max_reconnections = 4;
    static constexpr uint32_t reconnection_timeout_ms = 3000;
    static constexpr uint32_t dns_ttl = 3000;

    // Domain suffix for DNS name ==> http://eal.lan
    static constexpr const char* dns_tld = ".lan";

    ////// Runtime state with default values. This is overriden by NVS ///////

    // Run initially in access point mode when true
    bool ap_mode_active = true;
    // Auto-configure IP4 address in station mode when set to true
    bool sta_use_dhcp = true;

    // Activate DNS and/or MDNS service
    bool dns_active = true;
    bool mdns_active = false;

    char hostname[hostname_maxlen] = "eal";

    char ssid[ssid_maxlen] = "esp_ajax_lab";
    // Default value to be overridden with custom value on NVS.
    char psk[psk_maxlen] = "123FOO456";

    IPAddress ip4_addr = {192, 168, 4, 1};
    IPAddress ip4_gw = {192, 168, 4, 1};
    IPAddress ip4_mask = {255, 255, 0, 0};
};


/** @brief Constant / compile-time config values go here!
 */
struct AppConstants
{
    // Objects are constexpr, so members can be used as template parameters etc.
    constexpr AppConstants(){};

    ///////////////////////////// For AppController ///////////////////////////
    // App state serialization using the ArduionJSON module takes a lot of it
    uint32_t app_event_task_stack_size = 4096;
    // Arduino loop task has 1; async_tcp task has 3.
    // Assuming 2 is a good choice in-between..
    UBaseType_t app_event_task_priority = 2;
    // PRO_CPU_NUM == 1; APP_CPU_NUM == 0 on ESP32
    BaseType_t app_event_task_core_id = APP_CPU_NUM;
    // Fast timer for ADC conversion triggering etc
    uint32_t timer_fast_interval_ms = 20;
    /** @brief In addition to event-based async state update telegrams, we also
     * send cyclic updates to the HTTP client using this time interval (ms).
     */
    uint32_t timer_slow_interval_ms = 750;
    /** @brief Filename for persistent storage of runtime settings
     */
    const char *settings_filename = "/www/settings.json";

    ///////////////////////////// For ps_pwm C module: ////////////////////////
    // MCPWM unit can be [0,1]
    mcpwm_unit_t mcpwm_num = MCPWM_UNIT_0;
    // GPIO config for PWM output
    gpio_num_t gpio_pwm0a_out = GPIO_NUM_27; // PWM0A := LEAD leg, Low Side
    gpio_num_t gpio_pwm0b_out = GPIO_NUM_26; // PWM0B := LEAD leg, High Side
    gpio_num_t gpio_pwm1a_out = GPIO_NUM_25; // PWM1A := LAG leg, Low Side
    gpio_num_t gpio_pwm1b_out = GPIO_NUM_33; // PWM1B := LAG leg, High Side
    // Shutdown/fault input for PWM outputs
    gpio_num_t gpio_fault_shutdown = GPIO_NUM_4;
    // Active low / active high selection for fault input pin
    mcpwm_fault_input_level_t fault_pin_active_level = MCPWM_LOW_LEVEL_TGR;
    // Define here if the output pins shall be forced low or high
    // or high-impedance when a fault condition is triggered.
    // PWMxA and PWMxB have the same type of action, see declaration in mcpwm.h
    mcpwm_action_on_pwmxa_t disable_action_lag_leg = MCPWM_FORCE_MCPWMXA_LOW;
    // Lead leg might have a different configuration, e.g. stay at last output level
    mcpwm_action_on_pwmxa_t disable_action_lead_leg = MCPWM_FORCE_MCPWMXA_LOW;

    // Default runtime frequency setpoint limits
    float frequency_min = 50e3;
    float frequency_max = 300e3;
    // Initial frequency setpoint
    float init_frequency = 100e3;
    // Initial phase-shift setpoint
    float init_ps_duty = 0.45;
    // Initial leading leg dead-time value
    float init_lead_dt = 125e-9;
    // Initial lagging leg dead-time value
    float init_lag_dt = 125e-9;
    // Initial output state should be "false" representing "off"
    bool init_power_pwm_active = false;
};


/** @brief Hardware configuration for AuxHwDrv
 */
struct AuxHwDrvConfig
{
    // Objects are constexpr, so members can be used as template parameters etc.
    constexpr AuxHwDrvConfig(){};

    // Threshold values for activating/deactivating the fan in automatic mode
    float temp_1_fan_threshold_hi = 45.0;
    float temp_1_fan_threshold_lo = 40.0;
    float temp_2_fan_threshold_hi = 45.0;
    float temp_2_fan_threshold_lo = 40.0;
    // Analog inputs config //
    /** @brief ADC channel for first temperature sensor */
    adc1_channel_t temp_ch_1 = ADC1_CHANNEL_0; // Sensor VP
    /** @brief ADC channel for second temperature sensor */
    adc1_channel_t temp_ch_2 = ADC1_CHANNEL_3; // Sensor VN
    // Digital output GPIOs //
    gpio_num_t gpio_fan = GPIO_NUM_2;
    gpio_num_t gpio_overcurrent_reset = GPIO_NUM_16;
    gpio_num_t gpio_relay_ref = GPIO_NUM_18;
    gpio_num_t gpio_relay_dut = GPIO_NUM_19;
    gpio_num_t gpio_delta_sigma_out = GPIO_NUM_21;
    gpio_num_t gpio_drv_supply_en = GPIO_NUM_23;
    gpio_num_t gpio_drv_disable = GPIO_NUM_32;
    // Handled by LEDC PWM API
    gpio_num_t gpio_curr_limit_reference_pwm = GPIO_NUM_17;
    // GPIO config, inputs //
    gpio_num_t gpio_delta_sigma_in = GPIO_NUM_22;
    // Structures for GPIO and PWM API //
    gpio_config_t aux_periph_gpio_output_config {
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
    gpio_config_t aux_periph_gpio_input_config {
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
    ledc_timer_config_t pwm_timer_config {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 19500,
        .clk_cfg = LEDC_USE_APB_CLK // 80 MHz
    };
    ledc_channel_config_t curr_lim_pwm_ch_config {
        .gpio_num = gpio_curr_limit_reference_pwm,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    // Same PWM timer is used for isolated external delta_sigma hardware pin
    ledc_channel_config_t delta_sigma_out_pwm_ch_config {
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
    uint32_t oc_reset_pulse_length_ms = 20;
    // Calibration values for current limit PWM
    float curr_limit_pwm_scale = 1.0/100 * (
        1 << pwm_timer_config.duty_resolution);
    uint32_t curr_limit_pwm_offset = 0;
    ///////////////////////////////////////////////////////// End Configuration
};

#endif