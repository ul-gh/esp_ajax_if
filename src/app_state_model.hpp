/** @file app_state_model.hpp
 *
 * License: GPL v.3 
 * U. Lukas 2020-12-11
 */
#ifndef APP_STATE_MODEL__
#define APP_STATE_MODEL__

#include <ArduinoJson.h>

#include "ps_pwm.h"

/** Most default values are defined in app_config.hpp!
 */
#include "app_config.hpp"


/***************** AuxHwDrv state with initial values ******************//**
 * Used as public member of AuxHwDrv.
 * These members are accessed by AppController.
 */
struct AuxHwDrvState
{
    float current_limit = 8;
    bool relay_ref_active = false;
    bool relay_dut_active = false;
    float temp_1 = 150;
    float temp_2 = 150;
    float temp_1_limit = 50;
    float temp_2_limit = 50;
    bool fan_active = true;
    // Manual override, fan is permanently "on" when true
    bool fan_override = false;
    bool drv_supply_active = true;
    bool drv_disabled = false;
    // Overtemperature shutdown active flag
    bool hw_overtemp = true;
};


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
    // Initial values for AppController()
    static constexpr AppConstants constants{};

    /////////////////////// Runtime state starts here ///////////////////////

    // WiFi network configuration
    NetworkConfig net_conf{};

    // State from AuxHwDrv module
    AuxHwDrvState *aux_hw_drv_state = nullptr;

    // Setpoint throttling (rate of change limiting) takes place when enabled
    bool setpoint_throttling_enabled = true;
    // Internal setpoints of PSPWM C API
    pspwm_clk_conf_t* pspwm_clk_conf = nullptr;
    pspwm_setpoint_t* pspwm_setpoint = nullptr;
    pspwm_setpoint_limits_t* pspwm_setpoint_limits = nullptr;
    // Runtime user setpoint limits
    float frequency_min = constants.frequency_min;
    float frequency_max = constants.frequency_max;
    // Runtime setpoints and throttling increment per fast event timer interval
    float frequency_target = 100.0E3f;
    float frequency_increment = 500.0f;
    float duty_min = 0.0f;
    float duty_max = 0.8f;
    float duty_target = 0.0f;
    float duty_increment = 0.05f;
    // True when hardware OC shutdown condition is currently present
    bool hw_oc_fault_present = true;
    // Hardware Overcurrent Fault Shutdown Status is latched using this flag
    bool hw_oc_fault_occurred = true;
    // Pulse length for one-shot mode power output pulse
    uint32_t oneshot_power_pulse_length_ms = 1;


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
