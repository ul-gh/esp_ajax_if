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

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
static auto TAG = "AuxHwDrv";

/******************************** API *************************************//**
 */
AuxHwDrv::AuxHwDrv()
{
    ESP_LOGI(TAG, "Configuring auxiliary HW control module...");
    // Configure GPIO outputs
    gpio_config(&aux_hw_conf.aux_periph_gpio_output_config);
    // GPIO inputs
    gpio_config(&aux_hw_conf.aux_periph_gpio_input_config);

    // Configure PWM for current limit analog reference output
    ledc_timer_config(&aux_hw_conf.pwm_timer_config);
    ledc_channel_config(&aux_hw_conf.curr_lim_pwm_ch_config);
    
    // Set initial state for the outputs
    set_current_limit(state.current_limit);
    set_relay_ref_active(state.relay_ref_active);
    set_relay_dut_active(state.relay_dut_active);
    set_fan_active(state.fan_active);
    set_drv_supply_active(state.drv_supply_active);
    set_drv_disabled(state.drv_disabled);
}

AuxHwDrv::~AuxHwDrv(){
}

void AuxHwDrv::set_current_limit(float value) {
    ESP_LOGD(TAG, "Setting new current limit: %f", value);
    state.current_limit = value;

    auto timer_lpoint_compare_value =
        aux_hw_conf.curr_limit_pwm_offset
        + static_cast<uint32_t>(value * aux_hw_conf.curr_limit_pwm_scale);
    ledc_set_duty(aux_hw_conf.curr_lim_pwm_ch_config.speed_mode,
                  aux_hw_conf.curr_lim_pwm_ch_config.channel,
                  timer_lpoint_compare_value);
    ledc_update_duty(aux_hw_conf.curr_lim_pwm_ch_config.speed_mode,
                     aux_hw_conf.curr_lim_pwm_ch_config.channel);
}

void AuxHwDrv::set_relay_ref_active(bool new_state) {
    ESP_LOGD(TAG, "Setting relay REF active: %s", new_state ? "true" : "false");
    state.relay_ref_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_relay_ref),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_relay_dut_active(bool new_state) {
    ESP_LOGD(TAG, "Setting relay DUT active: %s", new_state ? "true" : "false");
    state.relay_dut_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_relay_dut),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_fan_active(bool new_state) {
    ESP_LOGD(TAG, "Setting fan active: %s", new_state ? "true" : "false");
    state.fan_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_fan),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_fan_override(bool new_state) {
    ESP_LOGD(TAG, "Setting fan manual override: %s", new_state ? "true" : "false");
    state.fan_override = new_state;
    if (new_state) {
        set_fan_active(true);
    }
}

void AuxHwDrv::set_drv_supply_active(bool new_state) {
    ESP_LOGD(TAG, "Setting driver supply active: %s", new_state ? "true" : "false");
    state.drv_supply_active = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_drv_supply_en),
                   static_cast<uint32_t>(new_state));
}

void AuxHwDrv::set_drv_disabled(bool new_state) {
    ESP_LOGD(TAG, "Setting driver disabled: %s", new_state ? "true" : "false");
    state.drv_disabled = new_state;
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_drv_disable),
                   static_cast<uint32_t>(new_state));
}

// Set GPIO for start of reset pulse
void AuxHwDrv::reset_oc_shutdown_start() {
    // OC detect reset line is high active, set it active
    ESP_LOGD(TAG, "Resetting overcurrent detect output! Setting reset pin high...");
    // Set pin high now. Pin must be reset later.
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_overcurrent_reset), 1);
}

// Reset GPIO for end of reset pulse
void AuxHwDrv::reset_oc_shutdown_finish() {
    // Set pin low again
    gpio_set_level(static_cast<gpio_num_t>(aux_hw_conf.gpio_overcurrent_reset), 0);
    ESP_LOGD(TAG, "Reset pin set low");
}

/* Get temperature sensor values via ADC, updates respective public attributes
 *
 * To be called periodically from fast timer event.
 */
void AuxHwDrv::update_temperature_sensors() {
    sensor_temp_1.update_filter();
    sensor_temp_2.update_filter();
    state.temp_1 = sensor_temp_1.get_temp_pwl();
    state.temp_2 = sensor_temp_2.get_temp_pwl();
}

/* Check if temperature exceeds threshold values, switch fan and
 * set overtemperature shutdown flag accordingly
 * 
 * To be called periodically from slow timer event
 */
void AuxHwDrv::evaluate_temperature_sensors() {
    if (       state.temp_1 > state.temp_1_limit
            || state.temp_2 > state.temp_2_limit) {
        state.hw_overtemp = true;
    }
    auto fan_active = state.fan_active;
    if (       state.temp_1 >= aux_hw_conf.temp_1_fan_threshold_hi
            || state.temp_2 >= aux_hw_conf.temp_2_fan_threshold_hi
            || state.fan_override) {
        fan_active = true;
    } else if (state.temp_1 < aux_hw_conf.temp_1_fan_threshold_lo
               && state.temp_2 < aux_hw_conf.temp_2_fan_threshold_lo) {
        fan_active = false;
    }
    if (fan_active != state.fan_active) {
        set_fan_active(fan_active);
    }
}
