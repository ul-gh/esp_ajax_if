#include "aux_hw_drv.hpp"
#include "info_debug_error.h"

 // These two lines might be unnecessary for C++17 and later standard   
constexpr ledc_timer_config_t AuxHwDrv::pwm_timer_config;
constexpr ledc_channel_config_t AuxHwDrv::pwm_channel_config;

AuxHwDrv::AuxHwDrv(float current_limit, bool relay_ref_active,
                   bool relay_dut_active, bool fan_active)
    : current_limit{current_limit}, relay_ref_active{relay_ref_active},
    relay_dut_active{relay_dut_active}, fan_active{fan_active}
    {

        debug_print("Configuring auxiliary HW control module...");

        // Test Overcurrent Reset pin
        pinMode(gpio_overcurrent_reset, OUTPUT);
        // FIXME: debug
        // Test Overcurrent Reset pin set to high
        digitalWrite(gpio_overcurrent_reset, HIGH);

        // Configure PWM for current limit analog reference output
        ledc_timer_config(&pwm_timer_config);
        ledc_channel_config(&pwm_channel_config);

        set_current_limit(current_limit);
        set_relay_ref_active(relay_ref_active);
        set_relay_dut_active(relay_dut_active);
        set_fan_active(fan_active);
    }

void AuxHwDrv::set_current_limit(float value) {
    debug_print_sv("Setting new current limit:", value);
    current_limit = value;

    uint32_t timer_lpoint_compare_value = curr_limit_pwm_offset + (uint32_t)(
        value * curr_limit_pwm_scale);
    ledc_set_duty(pwm_channel_config.speed_mode,
                  pwm_channel_config.channel,
                  timer_lpoint_compare_value);
    ledc_update_duty(pwm_channel_config.speed_mode, pwm_channel_config.channel);
}

void AuxHwDrv::set_relay_ref_active(bool state) {
    debug_print_sv("Setting relay REF active:", state);
    relay_ref_active = state;
}

void AuxHwDrv::set_relay_dut_active(bool state) {
    debug_print_sv("Setting relay DUT active:", state);
    relay_dut_active = state;
}

void AuxHwDrv::set_fan_active(bool state) {
    debug_print_sv("Setting fan active:", state);
    fan_active = state;
}
