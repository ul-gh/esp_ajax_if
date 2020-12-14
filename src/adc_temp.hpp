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


/** @brief Configuration constants and Look-Up-Table values which are
 * supposed to be the same for all sensors.
 * 
 * This is instantiated as a static constexpr shared by all sensor instances.
 */
struct KTY81_1xxCommonConfig
{
    ////////// If activated: Moving average filter length. Must be power of two.
    size_t moving_average_filter_len = 16;
    ////////// Configuration constants for get_kty_temp_lin()
    float temp_fsr_lower_lin = 0.0;
    float temp_fsr_upper_lin = 100.0;
    /** Voltages defining full-scale-range in mV */
    int32_t v_in_fsr_lower_lin = 886; // Corresponds to 0°C
    int32_t v_in_fsr_upper_lin = 1428; // Corresponds to 100°C

    ////////// Configuration constants for get_kty_temp_pwl()
    /** Voltages defining full-scale-range in mV */
    int32_t v_in_fsr_lower_lut = 596; // Corresponds to -55°C
    int32_t v_in_fsr_upper_lut = 1646; // Corresponds to 150°C
    /** @brief Look-Up-Table temperatures for 31 equidistant voltage steps.
     * Table only valid for linearised circuit using 2.2 kOhms series resistor
     * where ADC input voltage steps correspond to the following
     * temperature values in °C.
     * 
     * For LUT values, see ../util/kty81_1xx_sensor_generate_lut/kty81_lut.py
     */
    std::array<float, 32> lut_temp_kty81_121 {
    // For KTY81-121:
        -55.0       , -48.22273805, -41.51141124, -34.84623091,
        -28.34434926, -22.05459193, -15.78849403,  -9.53746745,
         -3.3772341 ,   2.7675195 ,   8.9372679 ,  15.0916243 ,
         21.14820431,  27.2082161 ,  33.34543424,  39.41134763,
         45.57173941,  51.73398583,  57.85244115,  64.10680179,
         70.45422093,  76.763773  ,  83.14712256,  89.64071316,
         96.17984636, 102.82297981, 109.58309561, 116.4296579 ,
        123.60532846, 131.27866698, 139.78106609, 150.0};
    // For KTY81-110 and KTY81-120:
    std::array<float, 32> lut_temp_kty81_110_120 {
        -55.0       , -48.16279303, -41.39749472, -34.8911357 ,
        -28.54294667, -22.192432  , -15.83544756,  -9.56004681,
         -3.43833483,   2.66313257,   8.80135444,  14.90432723,
         20.97767882,  27.03976174,  33.13792626,  39.28966437,
         45.38382931,  51.48407173,  57.67841773,  63.97159787,
         70.30279723,  76.61562129,  83.00362829,  89.50586837,
         96.07234208, 102.68301035, 109.39886725, 116.34253305,
        123.5137051 , 131.2558412 , 139.76912438, 150.0};
};


class ESP32ADC
{
public:
    /** @brief ADC configuration
     */
    static constexpr ESP32ADCConfig adc1_conf;

    ESP32ADC();

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
    void test_register_direct(void);


protected:
    esp_adc_cal_characteristics_t *adc_cal_characteristics{nullptr};
    U16MovingAverage<conf.moving_average_filter_1_len> filter_1;
    U16MovingAverage<conf.moving_average_filter_2_len> filter_2;

    /** @brief Initialisation of ADC, called by the constructor
     */
    void _init_test_capabilities(void);

    /** @brief Get raw ADC channel conversion value, repeats sampling
     * a number of times: "oversampling_ratio".
     */
    uint16_t _get_sample(adc1_channel_t channel);
};

#endif

// https://stackoverflow.com/questions/12117912/add-remove-data-members-with-template-parameters ?
class SensorKTY81_121
{
public:
    SensorKTY81_121(const ESP32ADC &adc, const adc1_channel_t channel);

    /** @brief Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
     * Use this if temperatures above 100°C ore below 0°C are to be measured.
     */
    float get_temp_pwl();

    /** @brief Fairly precise temperature conversion if the temperature sensor
     * voltage has good linearisation. Worst results at temperature extremes.
     */
    float get_temp_lin();

private:
    static constexpr KTY81_1xxCommonConfig _common_conf{};
    EquidistantPWL<_common_conf.v_in_fsr_lower_lut,
                   _common_conf.v_in_fsr_upper_lut,
                   _common_conf.lut_temp_kty81_121.size()> interpolator;
};

template<size_t filter_length>
class SensorKTY81_121_Filtered : public SensorKTY81_121
{
public:
    /** @brief Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
     * Use this if temperatures above 100°C ore below 0°C are to be measured.
     */
    float get_temp_pwl();

    /** Update the filter with new value read from ADC
     */
    void update_filter();

    // Suppose that does not make sense when going this length for precision
    float get_temp_lin() = delete;

private:
    U16MovingAverage<filter_length> filter;
};