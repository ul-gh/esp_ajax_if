/* esp32_adc_channel.cpp
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
#include <cmath>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "esp32_adc_channel.cpp";

#include "esp32_adc_channel.hpp"

ESP32ADCChannel::ESP32ADCChannel(adc1_channel_t channel_num,
                                 adc_atten_t attenuation,
                                 uint32_t averaged_samples,
                                 adc_bits_width_t bits_width,
                                 uint32_t default_vref)
    : channel_num{channel_num}
    , attenuation{attenuation}
{
    // Abort if N is not power of two and size limit is due to uint32_t sum
    if (averaged_samples > 1<<16 || (averaged_samples & (averaged_samples-1)) != 0) {
        ESP_LOGE(TAG, "Number must be power of two and smaller than 2^16");
        abort();
    }
    division_shift = log2(averaged_samples);
    // static member _bit_width is only then not equal to ADC_WIDTH_MAX if
    // this constructor was called before. Then all instances must be the same.
    if (_bits_width != ADC_WIDTH_MAX) {
        if (bits_width != _bits_width) {
            ESP_LOGE(TAG, "Bit width setting must be same for all channels");
            abort();
        }
    } else {
        _bits_width = bits_width;
        adc1_config_width(bits_width);
    }
    adc1_config_channel_atten(channel_num, attenuation);
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1, attenuation, bits_width, default_vref, &calibration_data);
    debug_print_characterisation_val_type(val_type);
    debug_print_check_efuse();
}

/* Get raw ADC channel conversion value, repeats sampling
 * a number of times as set per construction parameter "averaged_samples"
 * and returns the plain average.
 * 
 * The output is always scaled such as if the ADC was set to 12 bits mode,
 * i.e. theoretical full-scale output is 4096 - 1
 */
uint16_t ESP32ADCChannel::get_raw_averaged() {
    auto adc_reading = 0u;
    // Averaging seems necessary for the ESP32 ADC to obtain accurate results
    for (auto i=0u; i < (1u<<division_shift); i++) {
        adc_reading += adc1_get_raw(channel_num);
    }
    //Scale adc reading if ADC is not set to 12 bits mode
    adc_reading <<= ADC_WIDTH_BIT_12 - calibration_data.bit_width;
    // This is the division yielding the averaged result
    adc_reading >>= division_shift;
    //debug_print_sv("Raw ADC value:", adc_reading);
    //debug_print_sv("Fuse calibrated ADC conversion yields input voltage /mv:",
    //               esp_adc_cal_raw_to_voltage(adc_reading, adc_cal_characteristics));
    return static_cast<uint16_t>(adc_reading);
}

/* Get channel input voltage, this uses the averaged samples as
 * configured for get_raw_averaged() method.
 * 
 * This takes into account the calibration constants from ADC initialisation
 */
uint16_t ESP32ADCChannel::get_voltage_averaged() {
    return esp_adc_cal_raw_to_voltage(get_raw_averaged(), &calibration_data);
};

/* Calculate backwards the ADC reading for given input voltage,
 * based on calibration constants from ADC initialisation, and
 * also based on a ADC resolution setting of 12 bits.
 */
int32_t ESP32ADCChannel::adc_reading_from_voltage(uint32_t v_in_mv) {
    // Inversion of calculate_voltage_linear() function in esp_adc_cal.c
    // (((coeff_a * adc_reading) + LIN_COEFF_A_ROUND) / LIN_COEFF_A_SCALE) + coeff_b
    constexpr auto coeff_a_scale = 65536;
    constexpr auto coeff_a_round = coeff_a_scale/2;
    return (((v_in_mv - calibration_data.coeff_b) * coeff_a_scale)
            - coeff_a_round) / calibration_data.coeff_a;
}


void ESP32ADCChannel::debug_print_check_efuse(void) {
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Two Point: Supported");
    }
    else {
        ESP_LOGD(TAG, "eFuse Two Point: NOT supported");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Vref: Supported");
    }
    else {
        ESP_LOGD(TAG, "eFuse Vref: NOT supported");
    }
}

void ESP32ADCChannel::debug_print_characterisation_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGD(TAG, "Characterized using Two Point Value");
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGD(TAG, "Characterized using eFuse Vref");
    }
    else {
        ESP_LOGD(TAG, "Characterized using Default Vref");
    }
}

void ESP32ADCChannel::test_register_direct() {
    uint16_t adc_value;
    // only one channel is selected
    SENS.sar_meas_start1.sar1_en_pad = (1 << channel_num);
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    ESP_LOGD(TAG, "Register direct, sampled value: %d", adc_value);
}

