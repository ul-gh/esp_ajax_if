/** @file adc_filter.hpp
 * @brief Filter functions for ADC+DSP
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-11
 */
#ifndef ADC_FILTER_HPP__
#define ADC_FILTER_HPP__

#include <array>

/** @brief Does a recursive moving average over N values.
 * 
 * Filter length must be power of two and smaller than 2^16
 * 
 * This is a template class to allow the compiler to use bit shift operation
 * for the division and similar optimisations...
 */
template <size_t N>
class U16MovingAverage
{
public:
    // Checks N is power of two and size limit is due to uint32_t result_sum
    static_assert(N <= 1<<16 && (N & (N-1)) == 0);

    U16MovingAverage()
        : current_index{0}
    {}
    U16MovingAverage(uint16_t init_value)
        : current_index{0},
        input_buffer{init_value},
        result_sum{N * init_value}
    {}

    /** Initializes the whole buffer with one value
     * 
     * ==> First N calls of the filter yield no valid result
     */
    void initialize(uint16_t init_value) {
        input_buffer = {init_value};
        result_sum = {N * init_value};
    }

    uint16_t moving_average(uint16_t value_in) {
        uint16_t value_out = input_buffer[current_index];
        input_buffer[current_index] = value_in;
        current_index = (current_index + 1) % N;
        result_sum = result_sum + value_in - value_out;
        return result_sum / N;
    }

protected:
    size_t current_index;
    std::array<uint16_t, N> input_buffer;
    uint32_t result_sum;
};

#if false
/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 * 
 * LUT values representing function values starting with Y(X=in_fsr_lower)
 * and ending with Y(X=in_fsr_upper).
 * 
 * Y-values of the LUT must correspond to equidistant X-axis points.
 */
template<size_t N>
class EquidistantPiecewiseLinear
{
:public
    EquidistantPiecewiseLinear();

float pwl(const int32_t in_value);

, const int32_t in_fsr_lower,
        const int32_t in_fsr_upper, const std::array<const float, N> &lut_y) 

:private
    static constexpr lut_y{};

};
#endif

#endif