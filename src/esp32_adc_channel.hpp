/** @file esp32_adc_channel.hpp
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


/** @brief ESP32 ADC 1 channel access with configurable averaging.
 * 
 * Input voltage can be read raw or as calibrated voltage value in millivolts.
 * 
 * Raw output is always scaled such as if the ADC was set to 12 bits mode,
 * i.e. theoretical full-scale output is 4096 - 1
 * 
 * @note: For additional moving average filtering and smoothing, please see
 *        the additional templated version in class ESP32ADCChannelFiltered.
 */
class ESP32ADCChannel
{
public:
    adc1_channel_t channel_num;
    adc_atten_t attenuation;
    esp_adc_cal_characteristics_t calibration_data;

    /** @brief Initialize an ESP32 ADC channel.
     * 
     * @param channel_num: Analog input channel number
     * @param attenuation: Voltage input scale setting, see ESP-IDF reference
     * @param averaged_samples: Read this many input samples at once for any trigger
     * @param bits_width: Can be less then ADC_WIDTH_BIT_12 for faster speed.
     *        @note: The "bits_width" setting must be identical for all channels!
     * @param default_vref: Can be manually set if hardware has no e-fuse calibration
     */
    ESP32ADCChannel(adc1_channel_t channel_num,
                    adc_atten_t attenuation,
                    uint32_t averaged_samples = 64,
                    adc_bits_width_t bits_width = ADC_WIDTH_BIT_12,
                    uint32_t default_vref = 1100u);

    /** @brief Get raw ADC channel conversion value. Repeats sampling
     * a number of times, see "averaged_samples" constructor parameter
     * 
     * The output is always scaled such as if the ADC was set to 12 bits mode,
     * i.e. theoretical full-scale output is 4096 - 1
     */
    uint16_t get_raw_averaged();

    /** @brief Get channel input voltage in millivolts. Repeats sampling
     * a number of times, see "averaged_samples" constructor parameter
     * 
     * This takes into account the calibration constants from ADC initialisation.
     * If there is no e-fuse calibration on-chip, this uses the default_vref setting
     * from the constructor, default is 1100 millivolts.
     * 
     * @return: Voltage in millivolts
     */
    uint16_t get_voltage_averaged();

    /** @brief Calculate backwards the raw ADC reading for given input voltage,
     * based on calibration constants from ADC initialisation, and
     * also based on a ADC resolution setting of 12 bits.
     */
    int32_t calculate_raw_from_voltage(uint32_t v_in_mv);

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


/** @brief ESP32 ADC 1 channel access with configurable moving average filter.
 * Moving average filtering/smoothing is additional to the averaging of multiple
 * samples for each conversion trigger as per "averaged samples" parameter.
 * 
 * Input voltage can be read raw or as calibrated voltage value in millivolts.
 * 
 * Raw output is always scaled such as if the ADC was set to 12 bits mode,
 * i.e. theoretical full-scale output is 4096 - 1
 * @param filter_length: Template parameter setting the moving average length.
 *                       Must be a power of two and reasonable in size..
 */
template<size_t filter_length>
class ESP32ADCChannelFiltered : public ESP32ADCChannel
{
public:
    /** @brief Initialize an ESP32 ADC channel.
     * 
     * @param channel_num: Analog input channel number
     * @param attenuation: Voltage input scale setting, see ESP-IDF reference
     * @param averaged_samples: Read this many input samples at once for any trigger
     * @param bits_width: Can be less then ADC_WIDTH_BIT_12 for faster speed.
     *        @note: The "bits_width" setting must be identical for all channels!
     * @param default_vref: Can be manually set if hardware has no e-fuse calibration
     */
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

    /** @brief Explicitly trigger a new ADC analog input channel acquisition.
     * 
     * This updates the internal moving average filter.
     */
    void trigger_acquisition() {
        auto raw_sample = get_raw_averaged();
        filter.process_data(raw_sample);
    }

    /** @brief Get raw ADC channel conversion value, through moving average filter.
     * 
     * @param trigger_new_acquisition: If set to false, this does /not/ trigger
     * a new ADC acquisition but only returns the current filter result.
     * 
     * The output is always scaled such as if the ADC was set to 12 bits mode,
     * i.e. theoretical full-scale output is 4096 - 1
     */
    uint16_t get_raw_filtered(bool trigger_new_acquisition = true) {
        if (trigger_new_acquisition) {
            trigger_acquisition();
        }
        return filter.get_result();
    }


    /** @brief Get channel input voltage, filtered by internal moving average.
     * 
     * @param trigger_new_acquisition: If set to false, this does /not/ trigger
     * a new ADC acquisition but only returns the current filter result.
     * 
     * @return: Voltage in millivolts
     * 
     * This takes into account the calibration constants from ADC initialisation
     */
    uint16_t get_voltage_filtered(bool trigger_new_acquisition = true) {
        return esp_adc_cal_raw_to_voltage(get_raw_filtered(trigger_new_acquisition),
                                          &calibration_data);
    }

protected:
    U16MovingAverage<filter_length> filter;
};

#endif