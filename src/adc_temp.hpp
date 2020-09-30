/** @file adc_temp.hpp
 * @brief KTY81-121 type silicon temperature sensor readout and conversion
 * functions using the ESP32 ADC in its high-linearity region
 * 
 * Sensor connected between GND and ADC input and biased using a 2.2 kOhms
 * series-resistor connected to 3.3V supply.
 * 
 * Currently not implemented but useful addition would be ratiometric
 * measurement by additionally sampling the 3.3V supply.
 * 
 * Sensor readout with piecewise linear interpolation of LUT calibration values
 * or linear calculation as an option for lower precision applications
 * 
 * License: GPL v.3 
 * U. Lukas 2020-09-30
 */
#ifndef ADC_TEMP_HPP__
#define ADC_TEMP_HPP__

#include <array>

#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/sens_reg.h"
#include "soc/sens_struct.h"

namespace AdcTemp {
    /***************************** Configuration **************************//**
     **************************************************************************/
    static constexpr uint32_t default_vref{1100};
    static constexpr uint16_t oversampling_ratio{32};
    static constexpr adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;
    // Suggested ADC input voltage Range for ESP32 using ADC_ATTEN_DB_6 acc.
    // to the API reference for adc1_config_channel_atten() function is
    // 150 ~ 1750 millivolts. Max. FSR with reduced accuracy is approx. 2.2V
    static constexpr adc_atten_t temp_sense_attenuation = ADC_ATTEN_DB_6;
    static constexpr adc_unit_t unit = ADC_UNIT_1;

    /***************************** API ************************************//**
     **************************************************************************/
    constexpr adc1_channel_t temp_ch1 = ADC1_CHANNEL_0; // Sensor VP
    constexpr adc1_channel_t temp_ch2 = ADC1_CHANNEL_3; // Sensor VN

    /** @brief Initialisation of ADC
     * Must be called once before reading sensor values.
     */
    void adc_init_test_capabilities(void);

    /** @brief Perform complete readout of temperature sensor of temp_ch1
     * This uses the LUT for value conversion.
     */
    float get_aux_temp();

    /** @brief Perform complete readout of temperature sensor of temp_ch2
     * This uses the LUT for value conversion.
     */
    float get_heatsink_temp();

    /***************** Helper and debug functions *************************//**
     **************************************************************************/

    /** @brief Get raw ADC channel conversion value, for debugging and calibration
     */
    uint16_t adc_sample(adc1_channel_t channel);

    /** @brief Piecewise linear interpolation of look-up-table (LUT) values
     * representing function values starting with Y(X=in_fsr_lower)
     * and ending with Y(X=in_fsr_upper). Y-values of the LUT must correspond
     * to equidistant X-axis points.
     */
    float equidistant_piecewise_linear(
        const uint16_t in_value, const uint16_t in_fsr_lower,
        const uint16_t in_fsr_upper, const std::array<const float, 32> &lut_y);

    /** @brief Fairly precise temperature conversion if the temperature sensor
     * voltage has good linearisation. Does not work well at temperature extremes.
     */
    float get_kty_temp_lin(uint16_t adc_raw_value);

    /** @brief Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
     * Use this if temperatures above 100°C ore below 0°C are to be measured.
     */
    float get_kty_temp_pwl(uint16_t adc_raw_value);
}

#endif
