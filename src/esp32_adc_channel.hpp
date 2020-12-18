/** @file esp32_adc_channel.hpp
 * 
 * ESP32ADCChannel:
 * ESP32 ADC 1 channel configuration and access
 * 
 * ESP32ADCChannelFiltered<size_t filter_length>:
 * Templated variant featuring a moving average filter with compile-time
 * configurable size
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-16
 */
#ifndef ESP32_ADC_CHANNEL_HPP__
#define ESP32_ADC_CHANNEL_HPP__

#include <array>

#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/sens_reg.h"
#include "soc/sens_struct.h"

#include "adc_filter_interpolation.hpp"
#include "app_config.hpp"


class ESP32ADCChannel
{
public:
    adc1_channel_t channel_num;
    adc_atten_t attenuation;
    esp_adc_cal_characteristics_t calibration_data;

    /** @brief Initialise ADC channel.
     * 
     * The bits_width must be identical for all channels!
     */
    ESP32ADCChannel(adc1_channel_t channel_num,
                    adc_atten_t attenuation,
                    uint32_t averaged_samples = 64,
                    adc_bits_width_t bits_width = ADC_WIDTH_BIT_12,
                    uint32_t default_vref = 1100u);

    /** @brief Get raw ADC channel conversion value, repeats sampling
     * a number of times as set per construction parameter "averaged_samples"
     * and returns the plain average.
     * 
     * The output is always scaled such as if the ADC was set to 12 bits mode,
     * i.e. theoretical full-scale output is 4096 - 1
     */
    uint16_t get_raw_averaged();

    /** @brief Get channel input voltage, this uses the averaged samples as
     * configured for get_raw_averaged() method.
     * 
     * This takes into account the calibration constants from ADC initialisation
     */
    uint16_t get_voltage_averaged();

    /** @brief Calculate backwards the ADC reading for given input voltage,
     * based on calibration constants from ADC initialisation, and
     * also based on a ADC resolution setting of 12 bits.
     */
    int32_t adc_reading_from_voltage(uint32_t v_in_mv);

    /** Debug functions
     */
    void debug_print_check_efuse();
    void debug_print_characterisation_val_type(esp_adc_cal_value_t val_type);
    void test_register_direct();

protected:
    uint32_t division_shift;
    // Initialised with an invalid value to check if HW was initialised by a
    // previous constructor call. (All channels must have same bits_width)
    inline static auto _bits_width = adc_bits_width_t{ADC_WIDTH_MAX};
};


template<size_t filter_length>
class ESP32ADCChannelFiltered : public ESP32ADCChannel
{
public:
    ESP32ADCChannelFiltered(adc1_channel_t channel_num,
                            adc_atten_t attenuation,
                            uint32_t averaged_samples = 64,
                            adc_bits_width_t bits_width = ADC_WIDTH_BIT_12,
                            uint32_t default_vref = 1100u)
        : ESP32ADCChannel{channel_num, attenuation, averaged_samples,
                          bits_width, default_vref}
    {
        filter.initialize(get_raw_averaged());
    }

    void update_filter() {
        filter.moving_average(get_raw_averaged());
    }

    uint16_t get_filtered() {
        return filter.get_result();
    }

protected:
    U16MovingAverage<filter_length> filter;
};

#endif