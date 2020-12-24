/* sensor_kty81_1xx.cpp
 * 
 * Temperature sensor implementation using KTY81-1xx type analog
 * sensor and the ESP32 ADC 1
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-16
 */
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static auto TAG = "SensorKTY81_1xx";

#include "sensor_kty81_1xx.hpp"

SensorKTY81_1xx::SensorKTY81_1xx(adc1_channel_t channel,
                                 EquidistantPWLUInt16<_common_conf.lut_size> *interpolator)
    : adc_ch{channel, _common_conf.adc_ch_attenuation, _common_conf.averaged_samples}
    , _interpolator{interpolator}
{
    // Set up by derived classes SensorKTY81_121 and Sensor KTY81_110.
    assert(_interpolator);
    auto fsr_lower = adc_ch.calculate_raw_from_voltage(_common_conf.v_in_fsr_lower_lut);
    auto fsr_upper = adc_ch.calculate_raw_from_voltage(_common_conf.v_in_fsr_upper_lut);
    _interpolator->set_input_full_scale_range(fsr_lower, fsr_upper);
    ESP_LOGD(TAG, "adc_fsr_lower: %d", fsr_lower);
    ESP_LOGD(TAG, "adc_fsr_upper: %d", fsr_upper);
}

/* Updates the internal moving average filter with a new sampled value from ADC
 * 
 * This must be called periodically.
 */
void SensorKTY81_1xx::update_filter() {
    adc_ch.trigger_acquisition();
}

/* Excellent precision temperature sensing using piecewise linear
 * interpolation of Look-Up-Table values for a KTY81-121 type sensor.
 * Use this if temperatures above 100°C ore below 0°C are to be measured.
 */
float SensorKTY81_1xx::get_temp_pwl() {
    auto adc_raw = adc_ch.get_raw_filtered();
    return _interpolator->interpolate(adc_raw);
}

/* Fairly precise temperature conversion if the temperature sensor
 * voltage has good linearisation. Worst results at temperature extremes.
 */
float SensorKTY81_1xx::get_temp_lin() {
    auto fsr_lower = adc_ch.calculate_raw_from_voltage(_common_conf.v_in_fsr_lower_lin);
    auto fsr_upper = adc_ch.calculate_raw_from_voltage(_common_conf.v_in_fsr_upper_lin);
    constexpr auto temp_fsr = _common_conf.temp_fsr_upper_lin - _common_conf.temp_fsr_lower_lin;
    const auto temp_gain = temp_fsr / (fsr_upper - fsr_lower);
    auto raw_value = adc_ch.get_raw_filtered();
    return _common_conf.temp_fsr_lower_lin + temp_gain * (raw_value - fsr_lower);
}