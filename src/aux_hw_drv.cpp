#include "aux_hw_drv.hpp"
#include "info_debug_error.h"

AuxHwDrv::AuxHwDrv(float current_limit, bool relay_ref_active,
                   bool relay_dut_active, bool fan_active)
    : current_limit{current_limit}, relay_ref_active{relay_ref_active},
    relay_dut_active{relay_dut_active}, fan_active{fan_active}
    {
        debug_print("Configuring auxiliary HW control module...");
        // Overcurrent Reference PWM
        pinMode(gpio_curr_limit_reference_pwm, OUTPUT);
        // Test Overcurrent Reset pin
        pinMode(gpio_overcurrent_reset, OUTPUT);
        // FIXME: debug
        // Test Overcurrent Reference PWM set to high
        digitalWrite(gpio_curr_limit_reference_pwm, HIGH);
        // Test Overcurrent Reset pin set to high
        digitalWrite(gpio_overcurrent_reset, HIGH);

        set_current_limit(current_limit);
        set_relay_ref_active(relay_ref_active);
        set_relay_dut_active(relay_dut_active);
        set_fan_active(fan_active);
    }

void AuxHwDrv::set_current_limit(float value) {
    debug_print_sv("Setting new current limit:", value);
    current_limit = value;
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
