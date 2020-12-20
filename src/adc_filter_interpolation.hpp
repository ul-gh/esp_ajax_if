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
    {
        initialize(0);
    }
    U16MovingAverage(uint16_t init_value)
    {
        initialize(init_value);
    }

    /** @brief Initializes the moving average filter with a start value.
     * 
     * Called from constructor but can also be called on demand. 
     * 
     * @param init_value: Initial value
     * @note First N calls of the filter yield no completely filtered result
     *       but a varyingly weighted average of the init value and input data.
     * 
     */
    void initialize(uint16_t init_value) {
        input_buffer.fill(init_value);
        result_sum = N * init_value;
    }

    /** @brief Read in a new datum and update the filter.
     * 
     * @param value_in: Input datum, unsigned 16 bit
     * @return: Filtered result, unsigned 16 bit
     */
    uint16_t process_data(uint16_t value_in) {
        auto value_out = input_buffer[current_index];
        input_buffer[current_index] = value_in;
        current_index = (current_index + 1) % N;
        result_sum = result_sum + value_in - value_out;
        return result_sum / N;
    }

    /** @brief Get filter output value.
     * 
     * @return: Filtered result, unsigned 16 bit
     */
    uint16_t get_result() {
        return result_sum / N;
    }

protected:
    size_t current_index = 0;
    std::array<uint16_t, N> input_buffer;
    uint32_t result_sum;
};


/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 *
 * LUT values representing function values starting with y(x=in_fsr_lower)
 * and ending with y(x=in_fsr_upper).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 * 
 * This version has run-time settable input value range.
 */
template<uint32_t N>
struct EquidistantPWL
{
    std::array<float, N> _lut;
    int32_t in_fsr_lower;
    int32_t in_fsr_upper;
    float in_fsr_inv;

    EquidistantPWL(const std::array<float, N> &lut,
                   int32_t in_fsr_lower = 0,
                   int32_t in_fsr_upper = 1)
        : in_fsr_lower{in_fsr_lower}
        , in_fsr_upper{in_fsr_upper}
    {
        //_lut = lut; // Array copy
        for (auto i = 0u; i<_lut.size(); ++i) {
            _lut[i] = lut[i];
        }
        // I guess this is obvious: assert(in_fsr_upper - in_fsr_lower != 0);
        in_fsr_inv = 1.0f / (in_fsr_upper - in_fsr_lower);
    }

    float interpolate(int32_t x) {
        constexpr auto n_lut_intervals = N - 1;
        static_assert(n_lut_intervals > 0);
        int32_t lut_index;
        float partial_intervals;
        if (x > in_fsr_lower) {
            if (x < in_fsr_upper) {
            partial_intervals = in_fsr_inv * static_cast<float>(
                n_lut_intervals * (x - in_fsr_lower));
            // Rounding down gives number of whole intervals as index into the LUT
            lut_index = static_cast<int32_t>(partial_intervals);
            // By subtracting the whole intervals, only the partial rest remains
            partial_intervals -= lut_index;
            } else {
                lut_index = n_lut_intervals;
                partial_intervals = 0.0f;
            }
        } else {
            lut_index = 0;
            partial_intervals = 0.0f;
        }
        // Interpolation interval start and end values
        float interval_start = _lut[lut_index];
        float interval_end;
        if (lut_index < n_lut_intervals) {
            interval_end = _lut[lut_index + 1];
        } else {
            interval_end = interval_start;
        }
        return interval_start + partial_intervals * (interval_end - interval_start);
        }
};

/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 *
 * LUT values representing function values starting with y(x=in_fsr_lower)
 * and ending with y(x=in_fsr_upper).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 * 
 * This version is for compile-time-known input value range.
 */
template<int32_t FSR_LOWER, int32_t FSR_UPPER, uint32_t N>
struct EquidistantPWLTemplated
{
    std::array<float, N> _lut;

    EquidistantPWLTemplated(const std::array<float, N> &lut)
    {
        //_lut = lut; // Array copy
        for (auto i = 0u; i<_lut.size(); ++i) {
            _lut[i] = lut[i];
        }
    }

    float interpolate(int32_t x) {
        constexpr auto in_fsr_lower = FSR_LOWER;
        constexpr auto in_fsr_upper = FSR_UPPER;
        static_assert(in_fsr_upper - in_fsr_lower > 0);
        constexpr auto in_fsr_inv = 1.0f / (in_fsr_upper - in_fsr_lower);
        constexpr auto n_lut_intervals = _lut.size() - 1;
        static_assert(n_lut_intervals > 0);
        uint32_t lut_index;
        float partial_intervals;
        if (x > in_fsr_lower) {
            if (x < in_fsr_upper) {
            partial_intervals = in_fsr_inv * static_cast<float>(
                n_lut_intervals * (x - in_fsr_lower));
            // Rounding down gives number of whole intervals as index into the LUT
            lut_index = static_cast<int32_t>(partial_intervals);
            // By subtracting the whole intervals, only the partial rest remains
            partial_intervals -= lut_index;
            } else {
                lut_index = n_lut_intervals;
                partial_intervals = 0.0f;
            }
        } else {
            lut_index = 0;
            partial_intervals = 0.0f;
        }
        // Interpolation interval start and end values
        auto interval_start = _lut[lut_index];
        float interval_end;
        if (lut_index < n_lut_intervals) {
            interval_end = _lut[lut_index + 1];
        } else {
            interval_end = interval_start;
        }
        return interval_start + partial_intervals * (interval_end - interval_start);
        }
};

#endif