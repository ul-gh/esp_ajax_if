/* ADC access and KTY-xx type silicon temperature sensor conversion functions
 * for ESP32 HTTP-AJAX API module
 * 
 * License: GPL v.3 
 * U. Lukas 2020-09-20
 */
#include <array>
#include "adc_temp.hpp"
#include "esp_adc_cal.h"
#include "info_debug_error.h"

using namespace AdcTemp;

static esp_adc_cal_characteristics_t *adc_cal_characteristics;

static void check_efuse(void) {
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        Serial.print(F("eFuse Two Point: Supported\n"));
    }
    else {
        Serial.print(F("eFuse Two Point: NOT supported\n"));
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        Serial.print(F("eFuse Vref: Supported\n"));
    }
    else {
        Serial.print(F("eFuse Vref: NOT supported\n"));
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.print(F("Characterized using Two Point Value\n"));
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.print(F("Characterized using eFuse Vref\n"));
    }
    else {
        Serial.print(F("Characterized using Default Vref\n"));
    }
}

uint32_t adc_test_sample(void) {
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i=0; i < oversampling_ratio; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)temp_ch1);
    }
    adc_reading /= oversampling_ratio;

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_cal_characteristics);
    debug_print_sv("Raw ADC value:", adc_reading);
    debug_print_sv("Fuse calibration ADC input voltage /mv:", voltage);
    return adc_reading;
}

void adc_test_register_direct(void) {
    uint16_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << temp_ch1); // only one channel is selected
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    debug_print_sv("Register direct, sampled value:", adc_value);
}

/** Piecewise linear interpolation of input values using a look-up-table (LUT)
 * (lut_y) representing function values for X starting with Y(X=in_fsr_lower)
 * and ending with Y(X=in_fsr_upper). Y-values of the LUT must correspond
 * to equidistant X-axis points.
 */
static float equidistant_piecewise_linear(
        const uint32_t in_value, const uint32_t in_fsr_lower,
        const uint32_t in_fsr_upper, const std::array<const float, 32> &lut_y) {
    const uint32_t in_fsr = in_fsr_upper - in_fsr_lower;
    const uint32_t n_lut_intervals = lut_y.size() - 1;
    uint32_t lut_index;
    float partial_intervals;
    if (in_value > in_fsr_lower) {
        if (in_value < in_fsr_upper) {
        partial_intervals = n_lut_intervals * (in_value-in_fsr_lower) / in_fsr;
        debug_print_sv("Partial intervals:", partial_intervals);
        // Rounding down gives number of whole intervals as index into the LUT
        lut_index = static_cast<uint32_t>(partial_intervals);
        debug_print_sv("LUT index: ", lut_index);
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
    debug_print_sv("LUT value at index:", interval_start);
    float interval_end;
    if (lut_index < n_lut_intervals) {
        interval_end = lut_y[lut_index + 1];
    } else {
        interval_end = interval_start;
    }
    return interval_start + partial_intervals * (interval_end-interval_start);
}

static float get_kty_temp_lin(uint16_t adc_filtered_value) {
    constexpr uint16_t adc_fsr_lower = 0;
    constexpr uint16_t adc_fsr_upper = (1<<16) - 4000;
    constexpr uint16_t adc_fsr = adc_fsr_upper - adc_fsr_lower;
    constexpr float temp_offset = -20;
    constexpr float temp_gain = (150 -(-20)) / adc_fsr;
    return temp_offset + (adc_filtered_value - adc_fsr_lower) * temp_gain;
}

static float get_adc_ch_voltage(adc1_channel_t channel){
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < oversampling_ratio; i++)
    {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= oversampling_ratio;

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_cal_characteristics);
    Serial.printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
    return static_cast<float>(voltage) / 1000;
}


void AdcTemp::adc_init_test_capabilities(void) {
    check_efuse();

    adc1_config_width(bit_width);
    adc1_config_channel_atten(temp_ch1, temp_sense_attenuation);
    adc1_config_channel_atten(temp_ch2, temp_sense_attenuation);

    adc_cal_characteristics = static_cast<esp_adc_cal_characteristics_t *>(
        calloc(1, sizeof(esp_adc_cal_characteristics_t)));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        unit, temp_sense_attenuation, bit_width, default_vref, adc_cal_characteristics);
    print_char_val_type(val_type);
}

float AdcTemp::get_kty_temp_pwl() {
    constexpr int coeff_a_scale = 65536;
    constexpr int coeff_a_round = coeff_a_scale/2;
    // constexpr float r_s = 2200.0;
    // constexpr float v_cc = 3.3;
    // Voltages in mV!
    constexpr float v_in_fsr_lower = 0.596 * 1000;
    constexpr float v_in_fsr_upper = 1.646 * 1000;
    // Table only valid for linearised circuit using 2.2 kOhms series resistor
    // where 31 equidistant steps of output voltage correspond to the following
    // temperature values in °C. More than 32 values make no sense here.
    constexpr std::array<const float, 32> lut_y {
        -55.0, -48.21633884, -41.4355129, -34.82058319, -28.41377729,
        -22.14982999, -15.87831084, -9.61498446, -3.44681992, 2.69080486,
        8.8525326, 15.03360508, 21.15724271, 27.19458337, 33.28858446,
        39.46431559, 45.63181044, 51.77828562, 57.93204667, 64.14532885,
        70.45432559, 76.84910407, 83.21804075, 89.62168478, 96.23635248,
        102.8945437, 109.53437885, 116.34641919, 123.53946052, 131.23185819,
        139.76216906, 150.0};
    static_assert(lut_y.size() <= (1<<16), "LUT limited to max. 2^16 elements");
    // (((coeff_a * adc_reading) + LIN_COEFF_A_ROUND) / LIN_COEFF_A_SCALE) + coeff_b
    const uint32_t adc_fsr_lower = static_cast<uint32_t>(
        (((v_in_fsr_lower - adc_cal_characteristics->coeff_b)
          * coeff_a_scale)
         - coeff_a_round) / adc_cal_characteristics->coeff_a);
    const uint32_t adc_fsr_upper = static_cast<uint32_t>(
        (((v_in_fsr_upper - adc_cal_characteristics->coeff_b)
          * coeff_a_scale)
         - coeff_a_round) / adc_cal_characteristics->coeff_a);
    debug_print_sv("coeff_a:", adc_cal_characteristics->coeff_a);
    debug_print_sv("coeff_b:", adc_cal_characteristics->coeff_b);
    debug_print_sv("adc_fsr_lower: ", adc_fsr_lower);
    debug_print_sv("adc_fsr_upper: ", adc_fsr_upper);
    uint32_t adc_raw_filtered = adc_test_sample();
    return equidistant_piecewise_linear(
        adc_raw_filtered, adc_fsr_lower, adc_fsr_upper, lut_y);
}

float AdcTemp::get_aux_temp() {
    return 77.7;
    //return get_kty_temp_calc(get_adc_ch_voltage(temp_ch1));
}

float AdcTemp::get_heatsink_temp() {
    return 88.8;
    //return get_kty_temp_calc(get_adc_ch_voltage(temp_ch2));
}