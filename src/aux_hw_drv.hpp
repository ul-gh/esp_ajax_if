/** @file aux_hw_drv.hpp
 * @brief Auxiliary hardware driver for ESP-AJAX-Lab
 * 
 * U. Lukas 2020-09-30
 * License: GPL v.3
 */
#ifndef APP_HW_DRV_HPP__
#define APP_HW_DRV_HPP__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "adc_temp.hpp"

/** @brief Auxiliary hardware driver for ESP-AJAX-Lab
 * This implements GPIO control for relays, fan, enable and reset signals and
 * PWM generation used as a reference signal for hardware overcurrent limiter.
 * 
 * Further, temperature sensor readout is triggered here by calling
 * update_temperature_sensors() member peroiodically from timer task in
 * PsPWMAppHwControl::on_periodic_update_timer().
 * 
 * This class is also used as a container for its public attribute members
 * which represent the hardware state and are read-accessed externally.
 */
class AuxHwDrv
{
public:
    /***************** Configuration and default values ********************//**
     */
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

    /******************* Setpoints with initial values ********************//**
     * Also public for read-access by PsPwmAppHwControl module
     */
    float current_limit{0.0};
    bool relay_ref_active{false};
    bool relay_dut_active{false};
    float aux_temp{150};
    float heatsink_temp{150};
    bool fan_active{true};
    bool drv_supply_active{true};
    bool drv_disabled{false};

    /**************************** API *************************************//**
     */
    AuxHwDrv();
    virtual ~AuxHwDrv();

    void set_current_limit(float value);
    void set_relay_ref_active(bool state);
    void set_relay_dut_active(bool state);
    void set_fan_active(bool state);
    void set_drv_supply_active(bool state);
    void set_drv_disabled(bool state);
    void reset_oc_detect_shutdown(void (*callback)(void));
    void update_temperature_sensors();


private:
    TimerHandle_t oc_reset_oneshot_timer{NULL};
    void (*oc_reset_callback)(void);
    static void oc_reset_terminate_pulse(TimerHandle_t xTimer);
};

#endif