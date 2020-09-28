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
    static constexpr adc1_channel_t temp_ch1 = ADC1_CHANNEL_0; // Sensor VP
    static constexpr adc1_channel_t temp_ch2 = ADC1_CHANNEL_3; // Sensor VN
    static constexpr adc_bits_width_t bit_width = ADC_WIDTH_BIT_12;
    static constexpr adc_atten_t temp_sense_attenuation = ADC_ATTEN_DB_0;
    static constexpr adc_unit_t unit = ADC_UNIT_1;

    //// API
    void adc_init_test_capabilities(void);

    float get_kty_temp_pwl();

    float get_aux_temp();
    float get_heatsink_temp();
}

#endif