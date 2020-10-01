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
    /************************************************************************
     * Configuration
     */

    /** @brief ADC channel for AUX temperature sensor */
    constexpr adc1_channel_t temp_ch_aux = ADC1_CHANNEL_0; // Sensor VP
    /** @brief ADC channel for heatsink temperature sensor */
    constexpr adc1_channel_t temp_ch_heatsink = ADC1_CHANNEL_3; // Sensor VN

    ////////// Configuration constants for get_kty_temp_lin()
    static constexpr float temp_fsr_lower_lin = 0.0;
    static constexpr float temp_fsr_upper_lin = 100.0;
    /** Voltages defining full-scale-range in mV */
    static constexpr uint32_t v_in_fsr_lower_lin = 886; // Corresponds to 0°C
    static constexpr uint32_t v_in_fsr_upper_lin = 1428; // Corresponds to 100°C
    ////////// Configuration constants for get_kty_temp_pwl()
    /** Voltages defining full-scale-range in mV */
    static constexpr uint32_t v_in_fsr_lower_lut = 596; // Corresponds to -55°C
    static constexpr uint32_t v_in_fsr_upper_lut = 1646; // Corresponds to 150°C
    /** Table only valid for linearised circuit using 2.2 kOhms series resistor
     * where 31 equidistant steps of output voltage correspond to the following
     * temperature values in °C.
     */
    static constexpr std::array<const float, 32> lut_temp {
        -55.0, -48.21633884, -41.4355129, -34.82058319, -28.41377729,
        -22.14982999, -15.87831084, -9.61498446, -3.44681992, 2.69080486,
        8.8525326, 15.03360508, 21.15724271, 27.19458337, 33.28858446,
        39.46431559, 45.63181044, 51.77828562, 57.93204667, 64.14532885,
        70.45432559, 76.84910407, 83.21804075, 89.62168478, 96.23635248,
        102.8945437, 109.53437885, 116.34641919, 123.53946052, 131.23185819,
        139.76216906, 150.0};
    ////////// ADC hardware initialisation constants
    static constexpr uint32_t default_vref{1100};
    static constexpr uint16_t oversampling_ratio{32};
    static constexpr adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;
    /** Suggested ADC input voltage Range for ESP32 using ADC_ATTEN_DB_6 acc.
     * to the API reference for adc1_config_channel_atten() function is
     * 150 ~ 1750 millivolts. Max. FSR with reduced accuracy is approx. 2.2V
     */
    static constexpr adc_atten_t temp_sense_attenuation = ADC_ATTEN_DB_6;
    static constexpr adc_unit_t unit = ADC_UNIT_1;

    /************************************************************************
     * API
     */

    /** @brief Initialisation of ADC
     * Must be called once before reading sensor values.
     */
    void adc_init_test_capabilities(void);

    /** @brief Perform complete readout of temp_ch_aux
     * This uses the LUT for value conversion.
     */
    float get_aux_temp();

    /** @brief Perform complete readout of temp_ch_heatsink
     * This uses the LUT for value conversion.
     */
    float get_heatsink_temp();

    /************************************************************************
     * Helper and debug functions
     */

    /** Get raw ADC channel conversion value, for debugging and calibration
     */
    uint16_t adc_sample(adc1_channel_t channel);

    /** Piecewise linear interpolation of look-up-table (LUT) values
     * representing function values starting with Y(X=in_fsr_lower)
     * and ending with Y(X=in_fsr_upper). Y-values of the LUT must correspond
     * to equidistant X-axis points.
     */
    float equidistant_piecewise_linear(
        const uint16_t in_value, const uint16_t in_fsr_lower,
        const uint16_t in_fsr_upper, const std::array<const float, 32> &lut_y);

    /** Fairly precise temperature conversion if the temperature sensor voltage
     * has good linearisation. Does not work well at temperature extremes.
     */
    float get_kty_temp_lin(uint16_t adc_raw_value);

    /** Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
     * Use this if temperatures above 100°C ore below 0°C are to be measured.
     */
    float get_kty_temp_pwl(uint16_t adc_raw_value);
}

#endif
