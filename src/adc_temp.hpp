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

#include "adc_filter_interpolation.hpp"
#include "app_config.hpp"


class AdcTemp
{
public:
    static constexpr AdcTempConfig conf{};

    AdcTemp();

    /** @brief Initialisation of ADC
     * Must be called once before reading sensor values.
     */
    void adc_init_test_capabilities(void);

    /** @brief Perform complete readout of temp_ch_aux
     * This does a recursive moving average over N values
     * and uses the LUT for conversion of voltage readings to degrees celsius.
     */
    float get_heatsink_temp();

    /** @brief Perform complete readout of temp_ch_aux
     * This does a recursive moving average over N values
     * and uses the LUT for conversion of voltage readings to degrees celsius.
     */
    float get_aux_temp();

    /** @brief Piecewise linear interpolation of look-up-table (LUT) values
     * representing function values starting with Y(X=in_fsr_lower)
     * and ending with Y(X=in_fsr_upper). Y-values of the LUT must correspond
     * to equidistant X-axis points.
     */
    float equidistant_piecewise_linear(
        const int32_t in_value, const int32_t in_fsr_lower,
        const int32_t in_fsr_upper, const std::array<const float, 32> &lut_y);

    /** @brief Fairly precise temperature conversion if the temperature sensor voltage
     * has good linearisation. Does not work well at temperature extremes.
     */
    float get_kty_temp_lin(uint16_t adc_raw_value);

    /** @brief Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
     * Use this if temperatures above 100°C ore below 0°C are to be measured.
     */
    float get_kty_temp_pwl(uint16_t adc_raw_value);

    /** Some debug and helper functions
     */
    void check_efuse(void);
    void print_characterisation_val_type(esp_adc_cal_value_t val_type);
    void adc_test_register_direct(void);


protected:
    esp_adc_cal_characteristics_t *adc_cal_characteristics{nullptr};
    U16MovingAverage<conf.moving_average_filter_len> filter_1;
    U16MovingAverage<conf.moving_average_filter_len> filter_2;

    /** @brief Get raw ADC channel conversion value, repeats sampling
     * a number of times: "oversampling_ratio".
     */
    uint16_t _get_sample(adc1_channel_t channel);
};

#endif
