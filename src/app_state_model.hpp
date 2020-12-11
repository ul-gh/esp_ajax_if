/** @file app_state_model.hpp
 *
 * License: GPL v.3 
 * U. Lukas 2020-12-11
 */
#ifndef APP_STATE_MODEL__
#define APP_STATE_MODEL__

#include <ArduinoJson.h>

#include "ps_pwm.h"
#include "app_config.hpp"

/** @brief Application state containing data and settings model
 * 
 * Live data is kept here and and can be serialised to be sent to the
 * connected remote clients.
 * 
 * Runtime user configurable settings can be serialised and stored to file
 * or read back from file and restored into this instance.
 */
struct AppState
{
    // Configuration and initial values for the application state
    static constexpr AppConfig app_conf{};
    /** ATTENTION!
     * Following constants need to be adapted if JSON object size is changed!
     */
    static constexpr size_t _json_objects_size = JSON_OBJECT_SIZE(22);
    static constexpr size_t _strings_size = sizeof(
        "frequency_min_hw""frequency_max_hw""dt_sum_max_hw"
        "frequency_min""frequency_max"
        "frequency""duty""lead_dt""lag_dt""power_pwm_active"
        "current_limit""relay_ref_active""relay_dut_active"
        "aux_temp""heatsink_temp""fan_active"
        "base_div""timer_div"
        "drv_supply_active""drv_disabled"
        "hw_oc_fault_present""hw_oc_fault_occurred"
        );
    // Prevent buffer overflow even if above calculations are wrong...
    static constexpr size_t I_AM_SCARED_MARGIN = 50;
    static constexpr size_t json_size = _json_objects_size
                                        + _strings_size
                                        + I_AM_SCARED_MARGIN;

    /////////////////////// Runtime state starts here ///////////////////////
    // Zero-Copy values using pointers read from the PSPWM C API
    pspwm_clk_conf_t* pspwm_clk_conf = nullptr;
    pspwm_setpoint_t* pspwm_setpoint = nullptr;
    pspwm_setpoint_limits_t* pspwm_setpoint_limits = nullptr;
    // True when hardware OC shutdown condition is currently present
    bool hw_oc_fault_present = true;
    // Hardware Fault Shutdown Status is latched using this flag
    bool hw_oc_fault_occurred = true;
    // Runtime user settpoint limits for output frequency
    float frequency_min = app_conf.frequency_min;
    float frequency_max = app_conf.frequency_max;
    // Pulse length for one-shot mode power output pulse
    uint32_t oneshot_power_pulse_length_ms{0};

    // State from AuxHwDrv module
    AuxHwDrvState *aux_hw_drv_state = nullptr;

    /** @brief Application live data state in JSON format.
     * 
     * You must call serialize_data() before using the content.
     */
    char json_buf_data[json_size];

    /** @brief Application runtime configurable settings in JSON format.
     * 
     * You must call serialize_settings() before using the content.
     */
    char json_buf_settings[json_size];

    /** @brief Serialize application live data into json_buf_data
     */
    void serialize_data();

    /** @brief Serialize application runtime configurable settings into json_buf
     */
    void serialize_settings();

    /** @brief Restore application runtime configurable settings
     * from json_buf_settings back into this instance.
     */
    void deserialize_settings();

    /** @brief Write application runtime configurable settings as JSON to SPIFFs file.
     */
    bool save_to_file(const char *filename);

    /** @brief Restore application runtime configurable settings
     * from SPIFFs file back into this instance.
     */
    bool restore_from_file(const char *filename);

};

#endif