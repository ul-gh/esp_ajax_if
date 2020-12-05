/** Auxiliary hardware driver for ESP-AJAX-Lab
 * U. Lukas 2020-09-30
 * License: GPL v.3
 */
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include "soc/syscon_reg.h"

#include "aux_hw_drv.hpp"
#include "info_debug_error.h"

// Following out-of-class definitions of static constexpr data members are
// deprecated since C++17 (see: https://stackoverflow.com/a/57059723) but
// provided to prevent linker errors with current versions of the toolchain:
constexpr ledc_timer_config_t AuxHwDrvConfig::pwm_timer_config;
constexpr ledc_channel_config_t AuxHwDrvConfig::curr_lim_pwm_ch_config;
constexpr ledc_channel_config_t AuxHwDrvConfig::delta_sigma_out_pwm_ch_config;
constexpr gpio_config_t AuxHwDrvConfig::aux_periph_gpio_output_config;
constexpr gpio_config_t AuxHwDrvConfig::aux_periph_gpio_input_config;

/******************************** API *************************************//**
 */
AuxHwDrv::AuxHwDrv()
    {
        debug_print("Configuring auxiliary HW control module...");
        // Configure GPIO outputs
        gpio_config(&aux_hw_conf.aux_periph_gpio_output_config);
        // GPIO inputs
        gpio_config(&aux_hw_conf.aux_periph_gpio_input_config);

        // Initialize ADC
        debug_print("Initializing ADC...");
        AdcTemp::adc_init_test_capabilities();

        // Configure PWM for current limit analog reference output
        ledc_timer_config(&aux_hw_conf.pwm_timer_config);
        ledc_channel_config(&aux_hw_conf.curr_lim_pwm_ch_config);
        debug_print_sv("OC reset pin pulse length in timer ticks: ",
                       pdMS_TO_TICKS(aux_hw_conf.oc_reset_pulse_length_ms));

        // Configure FreeRTOS timer for generation of one-shot oc reset pulse
        oc_reset_oneshot_timer = xTimerCreate(
            "OC Reset Timer",
            pdMS_TO_TICKS(aux_hw_conf.oc_reset_pulse_length_ms),
            pdFALSE,
            static_cast<void*>(this),
            oc_reset_terminate_pulse
        );
        
        // Set initial state for the outputs
        set_current_limit(state.current_limit);
        set_relay_ref_active(state.relay_ref_active);
        set_relay_dut_active(state.relay_dut_active);
        set_fan_active(state.fan_active);
        set_drv_supply_active(state.drv_supply_active);
        set_drv_disabled(state.drv_disabled);
    }

AuxHwDrv::~AuxHwDrv(){
    xTimerDelete(oc_reset_oneshot_timer, 0);
}

void AuxHwDrv::set_current_limit(float value) {
    debug_print_sv("Setting new current limit:", value);
    state.current_limit = value;

    uint32_t timer_lpoint_compare_value = aux_hw_conf.curr_limit_pwm_offset + (uint32_t)(
        value * aux_hw_conf.curr_limit_pwm_scale);
    ledc_set_duty(aux_hw_conf.curr_lim_pwm_ch_config.speed_mode,
                  aux_hw_conf.curr_lim_pwm_ch_config.channel,
                  timer_lpoint_compare_value);
    ledc_update_duty(aux_hw_conf.curr_lim_pwm_ch_config.speed_mode,
                     aux_hw_conf.curr_lim_pwm_ch_config.channel);
}

void AuxHwDrv::set_relay_ref_active(bool new_state) {
    debug_print_sv("Setting relay REF active:", new_state);
    state.relay_ref_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_relay_ref),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_relay_dut_active(bool new_state) {
    debug_print_sv("Setting relay DUT active:", new_state);
    state.relay_dut_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_relay_dut),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_fan_active(bool new_state) {
    debug_print_sv("Setting fan active:", new_state);
    state.fan_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_fan),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_drv_supply_active(bool new_state) {
    debug_print_sv("Setting driver supply active:", new_state);
    state.drv_supply_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_drv_supply_en),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_drv_disabled(bool new_state) {
    debug_print_sv("Setting driver disabled:", new_state);
    state.drv_disabled = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_drv_disable),
                   static_cast<uint32_t>(new_state));
}

/* This needs a RTOS timer */
void AuxHwDrv::reset_oc_detect_shutdown(void (*callback)(void)) {
    // OC detect reset line is high active
    debug_print("Resetting overcurrent detect output!");
    oc_reset_callback = callback;
    // Set pin high now. Pin is reset by oc_reset_terminate_pulse() callback
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_overcurrent_reset), 1);
    BaseType_t errors;
    errors = xTimerReset(oc_reset_oneshot_timer,
                         pdMS_TO_TICKS(aux_hw_conf.oc_reset_pulse_length_ms));
    if (errors != pdPASS) {
        error_print("Overcurrent Reset Timer could not be started!");
    }
}

/* Get temperature sensor values via ADC, updates respective public attributes
 *
 * To be called periodically from PsPWMAppHwControl::on_periodic_update_timer().
 */
void AuxHwDrv::update_temperature_sensors() {
    state.aux_temp = AdcTemp::get_aux_temp();
    state.heatsink_temp = AdcTemp::get_heatsink_temp();
}


//////////////////////////////////////////////////////////////// Non-API private
// Static callback function for reset pulse timing
void AuxHwDrv::oc_reset_terminate_pulse(TimerHandle_t xTimer) {
    debug_print("Terminating rest pulse...");
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_overcurrent_reset), 0);
    AuxHwDrv* self = static_cast<AuxHwDrv*>(pvTimerGetTimerID(xTimer));
    debug_print("Calling oc_reset_callback");
    self->oc_reset_callback();
}