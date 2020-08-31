#ifndef APP_HW_DRV_HPP__
#define APP_HW_DRV_HPP__

#include "driver/ledc.h"
#include "esp_err.h"

class AuxHwDrv
{
public:
    //////////////////////////// Configuration
    // GPIO config
    static constexpr int gpio_relay_ref{};
    static constexpr int gpio_relay_dut{};
    static constexpr int gpio_fan{};
    static constexpr int gpio_curr_limit_reference_pwm{17};
    static constexpr int gpio_overcurrent_reset{16};
    // Configure PWM for current limit analog reference output.
    // maximum PWM frequency for given resolution in N bits is:
    // freq_hz = 80 MHz / 2^N
    static constexpr ledc_timer_config_t pwm_timer_config {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        {.duty_resolution = LEDC_TIMER_12_BIT},
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 19500
    };
    static constexpr ledc_channel_config_t pwm_channel_config {
        .gpio_num = gpio_curr_limit_reference_pwm,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    // Calibration values for current limit PWM
    static constexpr float curr_limit_pwm_scale = 1.0/100 * (
        1 << pwm_timer_config.duty_resolution);
    static constexpr uint32_t curr_limit_pwm_offset = 0;
    ////////////////////////////// End Configuration

    // Setpoints are public in order to be read-accessed by PsPwmAppHwControl
    float current_limit;
    bool relay_ref_active;
    bool relay_dut_active;
    bool fan_active;
    AuxHwDrv(float current_limit, bool relay_ref_active,
             bool relay_dut_active, bool fan_active);

    void set_current_limit(float value);
    void set_relay_ref_active(bool state);
    void set_relay_dut_active(bool state);
    void set_fan_active(bool state);

private:

};

#endif