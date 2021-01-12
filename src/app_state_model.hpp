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
    static constexpr size_t _json_objects_size = JSON_OBJECT_SIZE(24);
    static constexpr size_t _strings_size = sizeof(
        "frequency_min_hw""frequency_max_hw""dt_sum_max_hw"
        "frequency_min""frequency_max"
        "frequency""duty""lead_dt""lag_dt""power_pwm_active"
        "current_limit""relay_ref_active""relay_dut_active"
        "aux_temp""heatsink_temp""fan_active""fan_override"
        "base_div""timer_div"
        "drv_supply_active""drv_disabled"
        "hw_oc_fault_present""hw_oc_fault_occurred"
        "oneshot_len"
        );
    // Prevent buffer overflow even if above calculations are wrong...
    static constexpr size_t I_AM_SCARED_MARGIN = 50;
    static constexpr size_t json_buf_len = _json_objects_size
                                           + _strings_size
                                           + I_AM_SCARED_MARGIN;

    /////////////////////// Runtime state starts here ///////////////////////
    // Zero-Copy values using pointers read from the PSPWM C API
    pspwm_clk_conf_t* pspwm_clk_conf = nullptr;
    pspwm_setpoint_t* pspwm_setpoint = nullptr;
    pspwm_setpoint_limits_t* pspwm_setpoint_limits = nullptr;
    // Runtime user setpoint limits for output frequency
    float frequency_min = app_conf.frequency_min;
    float frequency_max = app_conf.frequency_max;
    // Setpoint throttling (limit rate of change)
    bool setpoint_throttling_active = true;
    float 
    // True when hardware OC shutdown condition is currently present
    bool hw_oc_fault_present = true;
    // Hardware Fault Shutdown Status is latched using this flag
    bool hw_oc_fault_occurred = true;
    // Pulse length for one-shot mode power output pulse
    uint32_t oneshot_power_pulse_length_ms{1};

    // State from AuxHwDrv module
    AuxHwDrvState *aux_hw_drv_state = nullptr;

    /** @brief Serialize application runtime state and configurable settings
     * into buffer as a JSON string.
     */
     size_t serialize_full_state(char *buf, size_t buf_len);

    /** @brief Restore application runtime configurable settings
     * from json string in buffer back into this instance.
     */
    bool deserialize_settings(const char *buf, size_t buf_len);

    /** @brief Write application runtime configurable settings
     * to SPIFFs file as a JSON string.
     */
    bool save_to_file(const char *filename);

    /** @brief Restore application runtime configurable settings
     * from SPIFFs file back into this instance.
     */
    bool restore_from_file(const char *filename);

};

#endif
