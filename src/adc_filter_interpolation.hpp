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

    /** @brief Initializes the whole buffer with one value
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


/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 *
 * LUT values representing function values starting with y(x=in_fsr_lower)
 * and ending with y(x=in_fsr_upper).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 */
template<int32_t FSR_LOWER, int32_t FSR_UPPER, uint32_t N>
struct EquidistantPWL
{
    const std::array<float, N> lut;

    EquidistantPWL(const std::array<float, N> &lut)
        :lut{lut}
    {}

    float interpolate(int32_t x) {
        constexpr int32_t in_fsr_lower{FSR_LOWER};
        constexpr int32_t in_fsr_upper{FSR_UPPER};
        constexpr int32_t in_fsr = in_fsr_upper - in_fsr_lower;
        constexpr int32_t n_lut_intervals = lut.size() - 1;
        static_assert(in_fsr > 0);
        static_assert(n_lut_intervals > 0);
        int32_t lut_index;
        float partial_intervals;
        if (x > in_fsr_lower) {
            if (x < in_fsr_upper) {
            partial_intervals = static_cast<float>(
                n_lut_intervals * (x - in_fsr_lower)
                ) / in_fsr;
            // Rounding down gives number of whole intervals as index into the LUT
            lut_index = static_cast<int32_t>(partial_intervals);
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
        float interval_start = lut[lut_index];
        float interval_end;
        if (lut_index < n_lut_intervals) {
            interval_end = lut[lut_index + 1];
        } else {
            interval_end = interval_start;
        }
        return interval_start + partial_intervals * (interval_end - interval_start);
        }
};

#endif