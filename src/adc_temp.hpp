#ifndef ADC_TEMP_HPP__
#define ADC_TEMP_HPP__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

#include "HardwareSerial.h"

#define DEFAULT_VREF 1100
#define NO_OF_SAMPLES 64

static esp_adc_cal_characteristics_t *adc_chars;
static const adc1_channel_t temp_ch1 = ADC1_CHANNEL_0; // Sensor VP
static const adc1_channel_t temp_ch2 = ADC1_CHANNEL_3; // Sensor VN

static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

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
    adc1_config_channel_atten(channel, atten);

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
        adc_reading += adc1_get_raw((adc1_channel_t)channel);
    }
    adc_reading /= NO_OF_SAMPLES;

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    Serial.printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
}

void adc_test_register_direct(void)
{
    uint16_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << channel); // only one channel is selected
    while (SENS.sar_slave_addr1.meas_status != 0);
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0);
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    Serial.printf("Register direct, sampled value: %d\n", adc_value);
}

#endif