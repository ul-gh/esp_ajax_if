/** @file aux_hw_drv.hpp
 * @brief Auxiliary hardware driver for ESP-AJAX-Lab
 * 
 * U. Lukas 2020-09-30
 * License: GPL v.3
 */
#ifndef APP_HW_DRV_HPP__
#define APP_HW_DRV_HPP__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "adc_temp.hpp"

#include "app_config.hpp"

/** @brief Auxiliary hardware driver for ESP-AJAX-Lab
 * This implements GPIO control for relays, fan, enable and reset signals and
 * PWM generation used as a reference signal for hardware overcurrent limiter.
 * 
 * Further, temperature sensor readout is triggered here by calling
 * update_temperature_sensors() member peroiodically from timer task in
 * PsPWMAppHwControl::on_periodic_update_timer().
 * 
 * This class is also used as a container for its public attribute members
 * which represent the hardware state and are read-accessed externally.
 */
class AuxHwDrv
{
public:
    static constexpr AuxHwDrvConfig aux_hw_conf{};
    AuxHwDrvState state;

    AuxHwDrv();
    virtual ~AuxHwDrv();

    void set_current_limit(float value);
    void set_relay_ref_active(bool state);
    void set_relay_dut_active(bool state);
    void set_fan_active(bool state);
    void set_drv_supply_active(bool state);
    void set_drv_disabled(bool state);
    static void reset_oc_shutdown_start();
    static void reset_oc_shutdown_finish();
    void update_temperature_sensors();


private:

};

#endif