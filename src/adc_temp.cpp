/** KTY81-121 type silicon temperature sensor readout and conversion
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
#include <cmath>

#include "esp_log.h"
static const char* TAG = "adc_temp.cpp";

#include "adc_temp.hpp"


auto ESP32ADCChannel::hardware_initialized = false;

ESP32ADCChannel::ESP32ADCChannel(adc1_channel_t channel_num,
                                 adc_atten_t attenuation,
                                 uint32_t averaged_samples,
                                 adc_bits_width_t bit_width,
                                 uint32_t default_vref)
    : channel_num{channel_num}
    , attenuation{attenuation}
{
    // Checks N is power of two and size limit is due to uint32_t sum
    if (averaged_samples > 1<<16 || (averaged_samples & (averaged_samples-1)) == 0) {
        ESP_LOGE(TAG, "Number must be power of two and smaller than 2^16");
        abort();
    }
    division_shift = log2(averaged_samples);
    if (hardware_initialized) {
        if (bit_width != calibration_data.bit_width) {
            ESP_LOGE(TAG, "Bit width setting must be same for all channels");
            abort();
        }
    } else {
        adc1_config_width(bit_width);
        hardware_initialized = true;
    }
    adc1_config_channel_atten(channel_num, attenuation);
    esp_adc_cal_characterize(
        ADC_UNIT_1, attenuation, bit_width, default_vref, &calibration_data);

}

/* Get raw ADC channel conversion value, repeats sampling
 * a number of times: "oversampling_ratio".
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

/* Get channel input voltage, this acquires a number of ADC samples
 * as per "get_averaged_samples" parameter and takes the average.
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
    constexpr int32_t coeff_a_scale = 65536;
    constexpr int32_t coeff_a_round = coeff_a_scale/2;
    return (((v_in_mv - calibration_data.coeff_b) * coeff_a_scale)
            - coeff_a_round) / calibration_data.coeff_a;
}


/** @brief Excellent precision temperature sensing using piecewise linear
 * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
 * Use this if temperatures above 100°C ore below 0°C are to be measured.
 */
float SensorKTY81_121::get_temp_pwl() {
    uint16_t raw_value = _get_sample(conf.temp_ch_heatsink);
    return get_kty_temp_pwl(filter_1.moving_average(raw_value));
}

/** @brief Fairly precise temperature conversion if the temperature sensor
 * voltage has good linearisation. Worst results at temperature extremes.
 */
float SensorKTY81_121::get_temp_lin() {
    uint16_t raw_value = _get_sample(conf.temp_ch_aux);
    return get_kty_temp_pwl(filter_2.moving_average(raw_value));
}


/** Fairly precise temperature conversion if the temperature sensor
 * voltage has good linearisation. Does not work well at temperature extremes.
 */
float AdcTemp::get_kty_temp_lin(uint16_t adc_raw_value) {
    assert(adc_cal_characteristics);
    constexpr int32_t coeff_a_scale = 65536;
    constexpr int32_t coeff_a_round = coeff_a_scale/2;
    const int32_t adc_fsr_lower = 
    const int32_t adc_fsr_upper = 
    constexpr float temp_fsr = conf.temp_fsr_upper_lin - conf.temp_fsr_lower_lin;
    const int32_t adc_fsr = adc_fsr_upper - adc_fsr_lower;
    const float temp_gain = temp_fsr / adc_fsr;
    return conf.temp_fsr_lower_lin + temp_gain * (adc_raw_value - adc_fsr_lower);
}

/** Excellent precision temperature sensing using piecewise linear
 * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
 * Use this if temperatures above 100°C ore below 0°C are to be measured.
 */
float AdcTemp::get_kty_temp_pwl(uint16_t adc_raw_value) {
    assert(adc_cal_characteristics);
    constexpr int32_t coeff_a_scale = 65536;
    constexpr int32_t coeff_a_round = coeff_a_scale/2;
    // In theory this could be up to 2^16 values but that would make no sense
    static_assert(conf.lut_temp.size() <= 64, "LUT limited to max. 64 elements");
    // Same as in calculate_voltage_linear() function in esp_adc_cal.c
    // (((coeff_a * adc_reading) + LIN_COEFF_A_ROUND) / LIN_COEFF_A_SCALE) + coeff_b
    const int32_t adc_fsr_lower = (
        ((conf.v_in_fsr_lower_lut - adc_cal_characteristics->coeff_b)
         * coeff_a_scale)
        - coeff_a_round) / adc_cal_characteristics->coeff_a;
    const int32_t adc_fsr_upper = (
        ((conf.v_in_fsr_upper_lut - adc_cal_characteristics->coeff_b)
         * coeff_a_scale)
        - coeff_a_round) / adc_cal_characteristics->coeff_a;
    //ESP_LOGD(TAG, "coeff_a: %d", adc_cal_characteristics->coeff_a);
    //ESP_LOGD(TAG, "coeff_b: %d", adc_cal_characteristics->coeff_b);
    //ESP_LOGD(TAG, "adc_fsr_lower: %d", adc_fsr_lower);
    //ESP_LOGD(TAG, "adc_fsr_upper: %d", adc_fsr_upper);
    return equidistant_piecewise_linear(
        adc_raw_value, adc_fsr_lower, adc_fsr_upper, conf.lut_temp);
}

void AdcTemp::check_efuse(void) {
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

void AdcTemp::print_characterisation_val_type(esp_adc_cal_value_t val_type) {
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

void AdcTemp::adc_test_register_direct(void) {
    uint16_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << conf.temp_ch_aux); // only one channel is selected
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    ESP_LOGD(TAG, "Register direct, sampled value: %d", adc_value);
}



/* Piecewise linear interpolation of look-up-table (LUT) values
 * representing function values starting with Y(X=in_fsr_lower)
 * and ending with Y(X=in_fsr_upper). Y-values of the LUT must
 * correspond to equidistant X-axis points.
 */
float AdcTemp::equidistant_piecewise_linear(
        const int32_t in_value, const int32_t in_fsr_lower,
        const int32_t in_fsr_upper, const std::array<const float, 32> &lut_y) {
    const int32_t in_fsr = in_fsr_upper - in_fsr_lower;
    const int32_t n_lut_intervals = lut_y.size() - 1;
    assert(in_fsr > 0);
    assert(n_lut_intervals > 0);
    int32_t lut_index;
    float partial_intervals;
    if (in_value > in_fsr_lower) {
        if (in_value < in_fsr_upper) {
        partial_intervals = static_cast<float>(
            n_lut_intervals * (in_value - in_fsr_lower)
            ) / in_fsr;
        // Rounding down gives number of whole intervals as index into the LUT
        lut_index = static_cast<int32_t>(partial_intervals);
        // By subtracting the whole intervals, only the partial rest remains
        partial_intervals -= lut_index;
        } else {
            lut_index = n_lut_intervals;
            partial_intervals = 0.0;
        }
    } else {
        lut_index = 0;
        partial_intervals = 0.0;
    }
    // Interpolation interval start and end values
    float interval_start = lut_y[lut_index];
    float interval_end;
    if (lut_index < n_lut_intervals) {
        interval_end = lut_y[lut_index + 1];
    } else {
        interval_end = interval_start;
    }
    return interval_start + partial_intervals * (interval_end - interval_start);
}