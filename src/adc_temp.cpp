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
#include "esp_log.h"
static const char* TAG = "AdcTemp";

#include "adc_temp.hpp"

// Following out-of-class definitions of static constexpr data members are
// deprecated since C++17 (see: https://stackoverflow.com/a/57059723) but
// provided to prevent linker errors with current versions of the toolchain:
constexpr AdcTempConfig AdcTemp::conf;


AdcTemp::AdcTemp() {
    adc_init_test_capabilities();
    filter_1.initialize(_get_sample(conf.temp_ch_heatsink));
    filter_2.initialize(_get_sample(conf.temp_ch_aux));
}

/** Initialisation of ADC
 * Must be called once before reading sensor values.
 */
void AdcTemp::adc_init_test_capabilities(void) {
    check_efuse();

    adc1_config_width(conf.bit_width);
    adc1_config_channel_atten(conf.temp_ch_heatsink, conf.temp_sense_attenuation);
    adc1_config_channel_atten(conf.temp_ch_aux, conf.temp_sense_attenuation);

    adc_cal_characteristics = static_cast<esp_adc_cal_characteristics_t *>(
        calloc(1, sizeof(esp_adc_cal_characteristics_t)));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        conf.unit, conf.temp_sense_attenuation, conf.bit_width,
        conf.default_vref, adc_cal_characteristics);
    print_characterisation_val_type(val_type);
}

/* Perform complete readout of temp_ch_heatsink
 * This does a recursive moving average over N=moving_average_filter_len values
 * and uses the LUT for conversion of voltage readings to degrees celsius.
 */
float AdcTemp::get_heatsink_temp() {
    uint16_t raw_value = _get_sample(conf.temp_ch_heatsink);
    return get_kty_temp_pwl(filter_1.moving_average(raw_value));
}

/* Perform complete readout of temp_ch_aux
 * This does a recursive moving average over N=moving_average_filter_len values
 * and uses the LUT for conversion of voltage readings to degrees celsius.
 */
float AdcTemp::get_aux_temp() {
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
    const int32_t adc_fsr_lower = (
        ((conf.v_in_fsr_lower_lin - adc_cal_characteristics->coeff_b)
         * coeff_a_scale)
        - coeff_a_round) / adc_cal_characteristics->coeff_a;
    const int32_t adc_fsr_upper = (
        ((conf.v_in_fsr_upper_lin - adc_cal_characteristics->coeff_b)
         * coeff_a_scale)
        - coeff_a_round) / adc_cal_characteristics->coeff_a;
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


/* Get raw ADC channel conversion value, repeats sampling
 * a number of times: "oversampling_ratio".
 */
uint16_t AdcTemp::_get_sample(adc1_channel_t channel) {
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i=0; i < conf.oversampling_ratio; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= conf.oversampling_ratio;
    //debug_print_sv("Raw ADC value:", adc_reading);
    //debug_print_sv("Fuse calibrated ADC conversion yields input voltage /mv:",
    //               esp_adc_cal_raw_to_voltage(adc_reading, adc_cal_characteristics));
    return static_cast<uint16_t>(adc_reading);
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