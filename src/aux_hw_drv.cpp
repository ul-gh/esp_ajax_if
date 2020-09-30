/** Auxiliary hardware driver for ESP-AJAX-Lab
 * U. Lukas 2020-09-30
 * License: GPL v.3
 */
#include "aux_hw_drv.hpp"
#include "info_debug_error.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include "soc/syscon_reg.h"


// Following redundant declarations of static constexpr data members are
// deprecated since C++17 but provided to prevent linker errors with
// outdated versions of the toolchain:
constexpr ledc_timer_config_t AuxHwDrv::pwm_timer_config;
constexpr ledc_channel_config_t AuxHwDrv::curr_lim_pwm_ch_config;
constexpr ledc_channel_config_t AuxHwDrv::delta_sigma_out_pwm_ch_config;
constexpr gpio_config_t AuxHwDrv::aux_periph_gpio_output_config;
constexpr gpio_config_t AuxHwDrv::aux_periph_gpio_input_config;

/******************************** API *************************************//**
 */
AuxHwDrv::AuxHwDrv()
    {
        debug_print("Configuring auxiliary HW control module...");
        // Configure GPIO outputs
        gpio_config(&aux_periph_gpio_output_config);
        // GPIO inputs
        gpio_config(&aux_periph_gpio_input_config);

        // Configure PWM for current limit analog reference output
        ledc_timer_config(&pwm_timer_config);
        ledc_channel_config(&curr_lim_pwm_ch_config);
        debug_print_sv("OC reset pin pulse length in timer ticks: ",
                       pdMS_TO_TICKS(oc_reset_pulse_length_ms));
        // Configure FreeRTOS timer for generation of one-shot oc reset pulse
        oc_reset_oneshot_timer = xTimerCreate(
            "OC Reset Timer",
            pdMS_TO_TICKS(oc_reset_pulse_length_ms),
            pdFALSE,
            static_cast<void*>(this),
            oc_reset_terminate_pulse
        );
        // Set initial state for the outputs
        set_current_limit(current_limit);
        set_relay_ref_active(relay_ref_active);
        set_relay_dut_active(relay_dut_active);
        set_fan_active(fan_active);
        set_drv_supply_active(drv_supply_active);
        set_drv_disabled(drv_disabled);
    }

AuxHwDrv::~AuxHwDrv(){
    xTimerDelete(oc_reset_oneshot_timer, 0);
}

void AuxHwDrv::set_current_limit(float value) {
    debug_print_sv("Setting new current limit:", value);
    current_limit = value;

    uint32_t timer_lpoint_compare_value = curr_limit_pwm_offset + (uint32_t)(
        value * curr_limit_pwm_scale);
    ledc_set_duty(curr_lim_pwm_ch_config.speed_mode,
                  curr_lim_pwm_ch_config.channel,
                  timer_lpoint_compare_value);
    ledc_update_duty(curr_lim_pwm_ch_config.speed_mode,
                     curr_lim_pwm_ch_config.channel);
}

void AuxHwDrv::set_relay_ref_active(bool state) {
    debug_print_sv("Setting relay REF active:", state);
    relay_ref_active = state;
    gpio_set_level(static_cast<gpio_num_t>(gpio_relay_ref),
                   static_cast<uint32_t>(state));
}

void AuxHwDrv::set_relay_dut_active(bool state) {
    debug_print_sv("Setting relay DUT active:", state);
    relay_dut_active = state;
    gpio_set_level(static_cast<gpio_num_t>(gpio_relay_dut),
                   static_cast<uint32_t>(state));
}

void AuxHwDrv::set_fan_active(bool state) {
    debug_print_sv("Setting fan active:", state);
    fan_active = state;
    gpio_set_level(static_cast<gpio_num_t>(gpio_fan),
                   static_cast<uint32_t>(state));
}

void AuxHwDrv::set_drv_supply_active(bool state) {
    debug_print_sv("Setting driver supply active:", state);
    drv_supply_active = state;
    gpio_set_level(static_cast<gpio_num_t>(gpio_drv_supply_en),
                   static_cast<uint32_t>(state));
}

void AuxHwDrv::set_drv_disabled(bool state) {
    debug_print_sv("Setting driver disabled:", state);
    drv_disabled = state;
    gpio_set_level(static_cast<gpio_num_t>(gpio_drv_disable),
                   static_cast<uint32_t>(state));
}

/* This needs a RTOS timer */
void AuxHwDrv::reset_oc_detect_shutdown(void (*callback)(void)) {
    // OC detect reset line is high active
    debug_print("Resetting overcurrent detect output!");
    oc_reset_callback = callback;
    // Set pin high now. Pin is reset by oc_reset_terminate_pulse() callback
    gpio_set_level(static_cast<gpio_num_t>(gpio_overcurrent_reset), 1);
    BaseType_t errors;
    errors = xTimerReset(oc_reset_oneshot_timer,
                         pdMS_TO_TICKS(oc_reset_pulse_length_ms));
    if (errors != pdPASS) {
        error_print("Overcurrent Reset Timer could not be started!");
    }
}

/* Get temperature sensor values via ADC, updates respective public attributes
 *
 * To be called periodically from PsPWMAppHwControl::on_periodic_update_timer().
 */
void AuxHwDrv::update_temperature_sensors() {
    aux_temp = AdcTemp::get_aux_temp();
    heatsink_temp = AdcTemp::get_heatsink_temp();
}


//////////////////////////////////////////////////////////////// Non-API private
// Static callback function for reset pulse timing
void AuxHwDrv::oc_reset_terminate_pulse(TimerHandle_t xTimer) {
    debug_print("Terminating rest pulse...");
    gpio_set_level(static_cast<gpio_num_t>(gpio_overcurrent_reset), 0);
    AuxHwDrv* self = static_cast<AuxHwDrv*>(pvTimerGetTimerID(xTimer));
    debug_print("Calling oc_reset_callback");
    self->oc_reset_callback();
}