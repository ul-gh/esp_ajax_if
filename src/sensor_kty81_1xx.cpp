/* sensor_kty81_1xx.cpp
 * 
 * Temperature sensor implementation using KTY81-1xx type analog
 * sensor and the ESP32 ADC 1
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-16
 */
#include "esp_log.h"
static const char* TAG = "SensorKTY81_1xx";

#include "sensor_kty81_1xx.hpp"

SensorKTY81_121::SensorKTY81_121(adc1_channel_t channel)
    : adc_ch{channel, _common_conf.adc_ch_attenuation, _common_conf.averaged_samples}
    , _interpolator{_common_conf.lut_temp_kty81_121}
{
    auto fsr_lower = adc_ch.adc_reading_from_voltage(_common_conf.v_in_fsr_lower_lut);
    auto fsr_upper = adc_ch.adc_reading_from_voltage(_common_conf.v_in_fsr_upper_lut);
    _interpolator.in_fsr_lower = fsr_lower;
    _interpolator.in_fsr_upper = fsr_upper;
    ESP_LOGD(TAG, "adc_fsr_lower: %d", fsr_lower);
    ESP_LOGD(TAG, "adc_fsr_upper: %d", fsr_upper);
}

/* Updates the moving average with a new sampled value from ADC
 * 
 * This must be called periodically.
 */
void SensorKTY81_121::update_filter() {
    adc_ch.update_filter();
}

/* Excellent precision temperature sensing using piecewise linear
 * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
 * Use this if temperatures above 100°C ore below 0°C are to be measured.
 */
float SensorKTY81_121::get_temp_pwl() {
    return _interpolator.interpolate(adc_ch.get_filtered());
}

/* Fairly precise temperature conversion if the temperature sensor
 * voltage has good linearisation. Worst results at temperature extremes.
 */
float SensorKTY81_121::get_temp_lin() {
    auto fsr_lower = adc_ch.adc_reading_from_voltage(_common_conf.v_in_fsr_lower_lin);
    auto fsr_upper = adc_ch.adc_reading_from_voltage(_common_conf.v_in_fsr_upper_lin);
    constexpr auto temp_fsr = _common_conf.temp_fsr_upper_lin - _common_conf.temp_fsr_lower_lin;
    const auto temp_gain = temp_fsr / (fsr_upper - fsr_lower);
    auto raw_value = adc_ch.get_filtered();
    return _common_conf.temp_fsr_lower_lin + temp_gain * (raw_value - fsr_lower);
}
