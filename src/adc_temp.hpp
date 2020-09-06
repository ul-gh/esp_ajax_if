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

static void check_efuse(void);

static void print_char_val_type(esp_adc_cal_value_t val_type);

void adc_init_test_capabilities(void);

void adc_test_sample(void);

void adc_test_register_direct(void);

float get_kty_temp(uint32_t adc_filtered);

#endif