#include <array>
#include "adc_temp.hpp"

static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        Serial.print(F("eFuse Two Point: Supported\n"));
    }
    else
    {
        Serial.print(F("eFuse Two Point: NOT supported\n"));
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        Serial.print(F("eFuse Vref: Supported\n"));
    }
    else
    {
        Serial.print(F("eFuse Vref: NOT supported\n"));
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        Serial.print(F("Characterized using Two Point Value\n"));
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        Serial.print(F("Characterized using eFuse Vref\n"));
    }
    else
    {
        Serial.print(F("Characterized using Default Vref\n"));
    }
}

void adc_init_test_capabilities(void)
{
    check_efuse();

    adc1_config_width(width);
    adc1_config_channel_atten(temp_ch1, atten);

    adc_chars = static_cast<esp_adc_cal_characteristics_t *>(
        calloc(1, sizeof(esp_adc_cal_characteristics_t)));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
}

void adc_test_sample(void)
{
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        adc_reading += adc1_get_raw((adc1_channel_t)temp_ch1);
    }
    adc_reading /= NO_OF_SAMPLES;

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    Serial.printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
}

void adc_test_register_direct(void)
{
    uint16_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << temp_ch1); // only one channel is selected
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    Serial.printf("Register direct, sampled value: %d\n", adc_value);
}

float u_16_piecewise_linear(uint16_t in_value) {
    constexpr uint16_t in_fsr_lower = 0;
    constexpr uint16_t in_fsr_upper = (1<<16) - 4000;
    constexpr uint16_t in_fsr = in_fsr_upper - in_fsr_lower;
    // temp_lut must represent equidistant definition range points
    constexpr std::array<const float, 3>lut {1.0, 2.0, 3.0};
    static_assert(lut.size() <= (1<<16), "LUT must have max. 2^16 elements!");
    constexpr uint32_t lut_intervals = lut.size() - 1;

    uint32_t lut_index;
    float lut_offset;
    if (in_value > in_fsr_lower) {
        if (in_value < in_fsr_upper) {
        uint32_t n_fsr_in = lut_intervals * (in_value - in_fsr_lower);
        lut_index =  n_fsr_in / in_fsr; // Floor division!
        lut_offset = static_cast<float>(n_fsr_in % in_fsr) / in_fsr;
        } else {
            lut_index = lut_intervals;
            lut_offset = 0.0;
        }
    } else {
        lut_index = 0;
        lut_offset = 0.0;
    }

    // Interpolation interval start and end values
    float interval_start = lut[lut_index];
    float interval_end;
    if (lut_index < lut_intervals) {
        interval_end = lut[lut_index + 1];
    } else {
        interval_end = interval_start;
    }

    return interval_start + lut_offset * (interval_end - interval_start);
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
    constexpr uint_16_t adc_fsr_lower = 0;
    constexpr uint_16_t adc_fsr_upper = (1<<16) - 4000;
    constexpr uint16_t adc_fsr = adc_fsr_upper - adc_fsr_lower;
    constexpr float v_adc_lower = 0.0;
    constexpr float v_adc_upper = 1.25;
    constexpr float v_adc_fsr = v_adc_upper - v_adc_lower;
    constexpr float r_v = 2.2e3;
    constexpr float v_cc = 3.3;
    float v_in = v_adc_lower + (adc_filtered_value - adc_fsr_lower)
                               * v_adc_fsr / adc_fsr;
    float r_kty = r_v * v_in / (v_cc - v_in);
    return float_piecewise_linear(r_kty);
}