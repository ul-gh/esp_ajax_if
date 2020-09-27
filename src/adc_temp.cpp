/* ADC access and KTY-xx type silicon temperature sensor conversion functions
 * for ESP32 HTTP-AJAX API module
 * 
 * License: GPL v.3 
 * U. Lukas 2020-09-20
 */
#include <array>
#include "adc_temp.hpp"

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

void adc_test_sample(void) {
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i=0; i < oversampling_ratio; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)temp_ch1);
    }
    adc_reading /= oversampling_ratio;

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_cal_characteristics);
    Serial.printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
}

void adc_test_register_direct(void) {
    uint16_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << temp_ch1); // only one channel is selected
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    Serial.printf("Register direct, sampled value: %d\n", adc_value);
}

/** Piecewise linear interpolation of input values using a look-up-table (LUT)
 * (lut_y) representing function values for X starting with Y(X=in_fsr_lower)
 * and ending with Y(X=in_fsr_upper). Y-values of the LUT must correspond
 * to equidistant X-axis points.
 */
float equidistant_piecewise_linear(uint32_t in_value) {
    constexpr uint32_t in_fsr_lower = 0;
    constexpr uint32_t in_fsr_upper = (1<<16) - 4000;
    constexpr uint32_t in_fsr = in_fsr_upper - in_fsr_lower;
    constexpr std::array<const float, 3>lut_y {22.0, 50.0, 88.0};
    static_assert(lut_y.size() <= (1<<16), "LUT must have max. 2^16 elements!");
    constexpr uint32_t n_lut_intervals = lut_y.size() - 1;
    uint32_t lut_index;
    float partial_intervals;
    if (in_value > in_fsr_lower) {
        if (in_value < in_fsr_upper) {
        partial_intervals = n_lut_intervals * (in_value-in_fsr_lower) / in_fsr;
        // Rounding down gives number of whole intervals as index into the LUT
        lut_index = static_cast<uint32_t>(partial_intervals);
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
    return interval_start + partial_intervals * (interval_end-interval_start);
}

float get_kty_temp_lin(uint16_t adc_filtered_value) {
    constexpr uint16_t adc_fsr_lower = 0;
    constexpr uint16_t adc_fsr_upper = (1<<16) - 4000;
    constexpr uint16_t adc_fsr = adc_fsr_upper - adc_fsr_lower;
    constexpr float temp_offset = -20;
    constexpr float temp_gain = (150 -(-20)) / adc_fsr;
    return temp_offset + (adc_filtered_value - adc_fsr_lower) * temp_gain;
}

float get_kty_temp_pwl(uint16_t adc_filtered_value) {
    constexpr uint16_t adc_fsr_lower = 0;
    constexpr uint16_t adc_fsr_upper = (1<<16) - 4000;
    constexpr uint16_t adc_fsr = adc_fsr_upper - adc_fsr_lower;
    constexpr float v_adc_lower = 0.0;
    constexpr float v_adc_upper = 1.25;
    constexpr float v_adc_fsr = v_adc_upper - v_adc_lower;
    constexpr float r_v = 2.2e3;
    constexpr float v_cc = 3.3;
    float v_in = v_adc_lower + (adc_filtered_value - adc_fsr_lower)
                               * v_adc_fsr / adc_fsr;
    uint32_t r_kty = r_v * v_in / (v_cc - v_in);
    return equidistant_piecewise_linear(r_kty);
}

float get_adc_ch_voltage(adc1_channel_t channel){
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

float AdcTemp::get_aux_temp() {
    return 77.7;
    //return get_kty_temp_calc(get_adc_ch_voltage(temp_ch1));
}

float AdcTemp::get_heatsink_temp() {
    return 88.8;
    //return get_kty_temp_calc(get_adc_ch_voltage(temp_ch2));
}