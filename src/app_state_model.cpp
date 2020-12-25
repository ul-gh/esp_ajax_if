#include <MD5Builder.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
static auto TAG = "app_state_model.cpp";

#include "fs_io.hpp"
#include "app_state_model.hpp"


/* Serialize application runtime state and configurable settings
 * into buffer as a JSON string.
 */
size_t AppState::serialize_full_state(char *buf, size_t buf_len) {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
    // ArduinoJson JsonDocument object, see https://arduinojson.org
    auto json_doc = StaticJsonDocument<_json_objects_size>{};
    // Setpoint limits from PSPWM hw constraints. Scaled to kHz, ns and % respectively...
    json_doc["frequency_min_hw"] = pspwm_setpoint_limits->frequency_min * 1e-3f;
    json_doc["frequency_max_hw"] = pspwm_setpoint_limits->frequency_max * 1e-3f;
    json_doc["dt_sum_max_hw"] = pspwm_setpoint_limits->dt_sum_max * 1e9f;
    // Runtime user setpoint limits for output frequency
    json_doc["frequency_min"] = frequency_min * 1e-3f;
    json_doc["frequency_max"] = frequency_max * 1e-3f;
    // Operational setpoints for PSPWM module
    json_doc["frequency"] = pspwm_setpoint->frequency * 1e-3f;
    json_doc["duty"] = pspwm_setpoint->ps_duty * 100.0f;
    json_doc["lead_dt"] = pspwm_setpoint->lead_red * 1e9f;
    json_doc["lag_dt"] = pspwm_setpoint->lag_red * 1e9f;
    json_doc["power_pwm_active"] = pspwm_setpoint->output_enabled;
    // Settings for auxiliary HW control module
    json_doc["current_limit"] = aux_hw_drv_state->current_limit;
    json_doc["relay_ref_active"] = aux_hw_drv_state->relay_ref_active;
    json_doc["relay_dut_active"] = aux_hw_drv_state->relay_dut_active;
    // Temperatures and fan
    json_doc["aux_temp"] = aux_hw_drv_state->aux_temp;
    json_doc["heatsink_temp"] = aux_hw_drv_state->heatsink_temp;
    json_doc["fan_active"] = aux_hw_drv_state->fan_active;
    json_doc["fan_override"] = aux_hw_drv_state->fan_override;
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
    // Length of the power output one-shot timer pulse
    json_doc["oneshot_len"] = oneshot_power_pulse_length_ms * 1e-3f;
    // Do the serialization
    return serializeJson(json_doc, buf, buf_len);
}

/* Restore application runtime configurable settings
 * from json string in buffer back into this instance.
 */
bool AppState::deserialize_settings(const char *buf, size_t buf_len) {
    assert(pspwm_clk_conf && pspwm_setpoint && pspwm_setpoint_limits && aux_hw_drv_state);
    // Size of the ArduinoJSON doc object must be larger here than when serializing..
    auto json_doc = StaticJsonDocument<json_buf_len>{};
    auto errors = deserializeJson(json_doc, buf, buf_len);
    if (errors != DeserializationError::Ok) {
        ESP_LOGE(TAG, "Error deserialising the JSON settings!\n Error code: %s",
                 errors.c_str());
        return false;
    }
    ///// We restore only a limited sub-set of all values...
    // Runtime user setpoint limits for output frequency
    frequency_min = float{json_doc["frequency_min"]} * 1e3f;
    frequency_max = float{json_doc["frequency_max"]} * 1e3f;
    // Operational setpoints for PSPWM module
    pspwm_setpoint->frequency = float{json_doc["frequency"]} * 1e3f;
    pspwm_setpoint->ps_duty = float{json_doc["duty"]} * 0.01f;
    pspwm_setpoint->lead_red = float{json_doc["lead_dt"]} * 1e-9f;
    pspwm_setpoint->lag_red = float{json_doc["lag_dt"]} * 1e-9f;
    // Settings for auxiliary HW control module
    aux_hw_drv_state->current_limit = float{json_doc["current_limit"]};
    aux_hw_drv_state->relay_ref_active = json_doc["relay_ref_active"];
    aux_hw_drv_state->relay_dut_active = json_doc["relay_dut_active"];
    // Temperatures and fan
    aux_hw_drv_state->fan_override = json_doc["fan_override"];
    // Clock divider settings
    pspwm_clk_conf->base_clk_prescale = uint8_t{json_doc["base_div"]};
    pspwm_clk_conf->timer_clk_prescale = uint8_t{json_doc["timer_div"]};
    // Length of the power output one-shot timer pulse
    oneshot_power_pulse_length_ms = static_cast<uint32_t>(
                                        float{json_doc["oneshot_len"]} * 1e3f);
    return true;
}

/* Write application runtime configurable settings as JSON to SPIFFs file.
 */
bool AppState::save_to_file(const char *filename) {
    auto json_buf = std::array<char, json_buf_len>{};
    auto json_size = serialize_full_state(json_buf.data(), json_buf_len);
    auto json_buf_uint8 = reinterpret_cast<uint8_t*>(json_buf.data());
    auto is_ok = FSIO::write_to_file_uint8(filename, json_buf_uint8, json_size);
    auto md5_builder = MD5Builder{};
    md5_builder.begin();
    md5_builder.add(json_buf_uint8, json_size);
    md5_builder.calculate();
    // MD5 sum of JSON string has length 32 + null byte = 33.
    auto json_md5 = std::array<char, 33>{};
    md5_builder.getChars(json_md5.data());
    ESP_LOGI(TAG, "MD5 sum: %s", json_md5.data());
    return is_ok;
}

/* Restore application runtime configurable settings
 * from SPIFFs file back into this instance.
 */
bool AppState::restore_from_file(const char *filename) {
    if (!SPIFFS.exists(filename)) {
        ESP_LOGI(TAG, "No stored settings found");
        return false;
    }
    auto json_buf = std::array<char, json_buf_len>{};
    auto json_buf_uint8 = reinterpret_cast<uint8_t*>(json_buf.data());
    auto len = FSIO::read_from_file_uint8(filename, json_buf_uint8, json_buf_len);
    return deserialize_settings(json_buf.data(), len);
}