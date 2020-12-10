#include <SPIFFS.h>
#include <FS.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "app_state_model.cpp";

#include "app_controller.hpp"

/* Serialize application live data into json_buf_data
 */
void AppState::serialize_data() {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
    // ArduinoJson JsonDocument object, see https://arduinojson.org
    StaticJsonDocument<_json_objects_size> json_doc;
    // Setpoint limits from PSPWM hw constraints. Scaled to kHz, ns and % respectively...
    json_doc["frequency_min_hw"] = pspwm_setpoint_limits->frequency_min/1e3;
    json_doc["frequency_max_hw"] = pspwm_setpoint_limits->frequency_max/1e3;
    json_doc["dt_sum_max_hw"] = pspwm_setpoint_limits->dt_sum_max*1e9;
    // Runtime user setpoint limits for output frequency
    json_doc["frequency_min"] = frequency_min/1e3;
    json_doc["frequency_max"] = frequency_max/1e3;
    // Operational setpoints for PSPWM module
    json_doc["frequency"] = pspwm_setpoint->frequency/1e3;
    json_doc["duty"] = pspwm_setpoint->ps_duty*100;
    json_doc["lead_dt"] = pspwm_setpoint->lead_red*1e9;
    json_doc["lag_dt"] = pspwm_setpoint->lag_red*1e9;
    json_doc["power_pwm_active"] = pspwm_setpoint->output_enabled;
    // Settings for auxiliary HW control module
    json_doc["current_limit"] = aux_hw_drv_state->current_limit;
    json_doc["relay_ref_active"] = aux_hw_drv_state->relay_ref_active;
    json_doc["relay_dut_active"] = aux_hw_drv_state->relay_dut_active;
    // Temperatures and fan
    json_doc["aux_temp"] = aux_hw_drv_state->aux_temp;
    json_doc["heatsink_temp"] = aux_hw_drv_state->heatsink_temp;
    json_doc["fan_active"] = aux_hw_drv_state->fan_active;
    // Clock divider settings
    json_doc["base_div"] = pspwm_clk_conf->base_clk_prescale;
    json_doc["timer_div"] = pspwm_clk_conf->timer_clk_prescale;
    // Gate driver supply and disable signals
    json_doc["drv_supply_active"] = aux_hw_drv_state->drv_supply_active;
    json_doc["drv_disabled"] = aux_hw_drv_state->drv_disabled;
    // True when hardware OC shutdown condition is present
    json_doc["hw_oc_fault_present"] = hw_oc_fault_present;
    // Hardware Fault Shutdown Status is latched using this flag
    json_doc["hw_oc_fault_occurred"] = hw_oc_fault_occurred;
    // Do the serialization
    serializeJson(json_doc, json_buf_data);
}

/* Serialize application runtime configurable settings into json_buf
 */
void AppState::serialize_settings() {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
    // ArduinoJson JsonDocument object, see https://arduinojson.org
    StaticJsonDocument<_json_objects_size> json_doc;
    // Setpoint limits from PSPWM hw constraints. Scaled to kHz, ns and % respectively...
    json_doc["frequency_min_hw"] = pspwm_setpoint_limits->frequency_min/1e3;
    json_doc["frequency_max_hw"] = pspwm_setpoint_limits->frequency_max/1e3;
    json_doc["dt_sum_max_hw"] = pspwm_setpoint_limits->dt_sum_max*1e9;
    // Runtime user setpoint limits for output frequency
    json_doc["frequency_min"] = frequency_min/1e3;
    json_doc["frequency_max"] = frequency_max/1e3;
    // Operational setpoints for PSPWM module
    json_doc["frequency"] = pspwm_setpoint->frequency/1e3;
    json_doc["duty"] = pspwm_setpoint->ps_duty*100;
    json_doc["lead_dt"] = pspwm_setpoint->lead_red*1e9;
    json_doc["lag_dt"] = pspwm_setpoint->lag_red*1e9;
    json_doc["power_pwm_active"] = pspwm_setpoint->output_enabled;
    // Settings for auxiliary HW control module
    json_doc["current_limit"] = aux_hw_drv_state->current_limit;
    json_doc["relay_ref_active"] = aux_hw_drv_state->relay_ref_active;
    json_doc["relay_dut_active"] = aux_hw_drv_state->relay_dut_active;
    // Temperatures and fan
    json_doc["fan_active"] = aux_hw_drv_state->fan_active;
    // Clock divider settings
    json_doc["base_div"] = pspwm_clk_conf->base_clk_prescale;
    json_doc["timer_div"] = pspwm_clk_conf->timer_clk_prescale;
    // Do the serialization
    serializeJson(json_doc, json_buf_settings);
}

/* Restore application runtime configurable settings
 * from json_buf_settings back into this instance.
 */
void AppState::deserialize_settings() {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
}

/* Write application runtime configurable settings as JSON to SPIFFs file.
 */
bool AppState::save_to_file(const char *filename) {
    if (!SPIFFS.begin(false)) {
        ESP_LOGE(TAG, "Could not open SPIFFS!");
        return false;
    }
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "Could not open file for writing!: %s", filename);
        return false;
    }
    return true;
}

/* Restore application runtime configurable settings
 * from SPIFFs file back into this instance.
 */
bool AppState::restore_from_file(const char *filename) {
    if (!SPIFFS.begin(false)) {
        ESP_LOGE(TAG, "Could not open SPIFFS!");
        return false;
    } else if (!SPIFFS.exists(filename)) {
        ESP_LOGE(TAG, "File does not exist: %s", filename);
        return false;
    }
    File file = SPIFFS.open(filename, "r");
    if (!file) {
        ESP_LOGE(TAG, "Could not open file!: %s", filename);
        return false;
    }
    return true;
}