#ifndef ADC_TEMP_HPP__
#define ADC_TEMP_HPP__

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

#include "HardwareSerial.h"

namespace AdcTemp {
    //// Configuration
    static constexpr uint32_t default_vref{1100};
    static constexpr uint16_t oversampling_ratio{32};
    static constexpr adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;
    // Suggested ADC input voltage Range for ESP32 using ADC_ATTEN_DB_6 acc.
    // to the API reference for adc1_config_channel_atten() function is
    // 150 ~ 1750 millivolts. Max. FSR with reduced accuracy is approx. 2.2V
    static constexpr adc_atten_t temp_sense_attenuation = ADC_ATTEN_DB_6;
    static constexpr adc_unit_t unit = ADC_UNIT_1;

    //// API
    constexpr adc1_channel_t temp_ch1 = ADC1_CHANNEL_0; // Sensor VP
    constexpr adc1_channel_t temp_ch2 = ADC1_CHANNEL_3; // Sensor VN

    void adc_init_test_capabilities(void);

    uint16_t adc_sample(adc1_channel_t channel);

    /** Fairly precise temperature conversion if the temperature sensor
     * voltage has good linearisation
     */
    float get_kty_temp_lin(uint16_t adc_raw_value);

    /** Excellent precision temperature sensing using piecewise linear
     * interpolation of Look-Up-Table values for a KTY81-121 type sensor
     */
    float get_kty_temp_pwl(uint16_t adc_raw_value);

    float get_aux_temp();
    float get_heatsink_temp();
}

#endif