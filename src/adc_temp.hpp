/** @file adc_temp.hpp
 * @brief KTY81-1xx type silicon temperature sensor readout and conversion
 * functions using the ESP32 ADC in its high-linearity region
 * 
 * Sensor connected between GND and ADC input and biased using a 2.2 kOhms
 * series-resistor connected to 3.3V supply.
 * 
 *       +-----------------------+
 *       |                       |
 *      +++                      |VREF
 *      | |r_pullup              |(3V3)
 *      | |(2k2)             +----------+
 *      +++                  |          |
 *       |                   |          |
 *       +-------------+-----+ AIN      |
 *       |             |     |          |
 *      +++            |     +---+------+
 *      | |KTY81-    +---+       | AGND
 *      | | 1xx      +---+       |
 *      +++            |100nF    |
 *       |             |         |
 *       +-------------+---------+
 *
 * Currently not implemented but useful addition would be ratiometric
 * measurement by additionally sampling the 3.3V reference/supply.
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
    static constexpr int32_t v_in_fsr_lower_lin = 886; // Corresponds to 0°C
    static constexpr int32_t v_in_fsr_upper_lin = 1428; // Corresponds to 100°C
    ////////// Configuration constants for get_kty_temp_pwl()
    /** Voltages defining full-scale-range in mV */
    static constexpr int32_t v_in_fsr_lower_lut = 596; // Corresponds to -55°C
    static constexpr int32_t v_in_fsr_upper_lut = 1646; // Corresponds to 150°C
    /** @brief Look-Up-Table temperatures for 31 equidistant voltage steps.
     * Table only valid for linearised circuit using 2.2 kOhms series resistor
     * where ADC input voltage steps correspond to the following
     * temperature values in °C.
     * 
     * For LUT values, see ../util/kty81_1xx_sensor_generate_lut/kty81_lut.py
     */
    // For KTY81-121:
    static constexpr std::array<const float, 32> lut_temp {
        -55.0       , -48.22273805, -41.51141124, -34.84623091,
        -28.34434926, -22.05459193, -15.78849403,  -9.53746745,
         -3.3772341 ,   2.7675195 ,   8.9372679 ,  15.0916243 ,
         21.14820431,  27.2082161 ,  33.34543424,  39.41134763,
         45.57173941,  51.73398583,  57.85244115,  64.10680179,
         70.45422093,  76.763773  ,  83.14712256,  89.64071316,
         96.17984636, 102.82297981, 109.58309561, 116.4296579 ,
        123.60532846, 131.27866698, 139.78106609, 150.0};
    // For KTY81-110 and KTY81-120:
    //static constexpr std::array<const float, 32> lut_temp {
    //    -55.0       , -48.16279303, -41.39749472, -34.8911357 ,
    //    -28.54294667, -22.192432  , -15.83544756,  -9.56004681,
    //     -3.43833483,   2.66313257,   8.80135444,  14.90432723,
    //     20.97767882,  27.03976174,  33.13792626,  39.28966437,
    //     45.38382931,  51.48407173,  57.67841773,  63.97159787,
    //     70.30279723,  76.61562129,  83.00362829,  89.50586837,
    //     96.07234208, 102.68301035, 109.39886725, 116.34253305,
    //    123.5137051 , 131.2558412 , 139.76912438, 150.0};
    ////////// ADC hardware initialisation constants
    static constexpr uint32_t default_vref{1100};
    static constexpr uint16_t oversampling_ratio{64};
    static constexpr adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;
    /** @brief Suggested ADC input voltage Range for ESP32 using ADC_ATTEN_DB_6
     * is 150 ~ 1750 millivolts according to the SDK documentation for function
     * adc1_config_channel_atten(). With reduced accuracy, FSR is approx. 2.2V.
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
        const int32_t in_value, const int32_t in_fsr_lower,
        const int32_t in_fsr_upper, const std::array<const float, 32> &lut_y);

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
