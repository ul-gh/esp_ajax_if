/** @file adc_filter.hpp
 * @brief Filter functions for ADC+DSP
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-11
 */
#ifndef ADC_FILTER_HPP__
#define ADC_FILTER_HPP__

#include <array>
#include "esp_log.h"


/** @brief Does a recursive moving average over N values.
 * 
 * Filter length must be power of two and smaller than 2^16
 * 
 * This is a template class to have a compile-time-known fixed divisor, allowing
 * the compiler to use fast bit shift operation instead of int division.
 */
template <size_t N>
class MovingAverageUInt16
{
public:
    // Checks N is power of two and size limit is due to uint32_t result_sum
    static_assert(N <= 1<<16 && (N & (N-1)) == 0);

    MovingAverageUInt16()
    {
        initialize(0);
    }
    MovingAverageUInt16(uint16_t init_value)
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
     */
    void input_data(uint16_t value_in) {
        auto value_out = input_buffer[current_index];
        input_buffer[current_index] = value_in;
        current_index = (current_index + 1) % N;
        result_sum = result_sum + value_in - value_out;
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
 * LUT values representing function values starting with y(x=in_fsr_bot)
 * and ending with y(x=in_fsr_top).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 * 
 * Version for int32_t input value.
 */
template<size_t N>
class EquidistantPWLInt32
{
public:
    EquidistantPWLInt32(const std::array<float, N> &lut,
                   int32_t in_fsr_bot = 0,
                   int32_t in_fsr_top = 1)
        : _lut{lut}
    {
        set_input_full_scale_range(in_fsr_bot, in_fsr_top);
    }

    void set_input_full_scale_range(int32_t in_fsr_bot, int32_t in_fsr_top) {
        assert(in_fsr_top > in_fsr_bot);
        _in_fsr_bot = in_fsr_bot;
        _in_fsr_top = in_fsr_top;
        _in_fsr_inv = 1.0f / (in_fsr_top - in_fsr_bot);
    }

    float interpolate(int32_t x) {
        static_assert(1 < N && N < INT32_MAX, "Filter length must be in this range");
        constexpr auto n_lut_intervals = size_t{N - 1};
        size_t lut_index;
        float partial_intervals;
        if (x > _in_fsr_bot) {
            if (x < _in_fsr_top) {
                auto n = static_cast<int64_t>(n_lut_intervals) * (x - _in_fsr_bot);
                auto d = _in_fsr_top - _in_fsr_bot;
                auto quot_rem = lldiv(n, d);
                partial_intervals = _in_fsr_inv * static_cast<float>(quot_rem.rem);
                lut_index = static_cast<size_t>(quot_rem.quot);
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

protected:
    const std::array<float, N> _lut;
    int32_t _in_fsr_bot;
    int32_t _in_fsr_top;
    float _in_fsr_inv;
};


/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 *
 * LUT values representing function values starting with y(x=in_fsr_bot)
 * and ending with y(x=in_fsr_top).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 * 
 * Version for uint16_t input value.
 */
template<size_t N>
class EquidistantPWLUInt16
{
public:
    EquidistantPWLUInt16(const std::array<float, N> &lut,
                   uint16_t in_fsr_bot = 0,
                   uint16_t in_fsr_top = 1)
        : _lut{lut}
    {
        set_input_full_scale_range(in_fsr_bot, in_fsr_top);
    }

    void set_input_full_scale_range(uint16_t in_fsr_bot, uint16_t in_fsr_top) {
        assert(in_fsr_top > in_fsr_bot);
        // 32-bit is native and fast int, also avoid unsigned int promotion
        _in_fsr_bot = in_fsr_bot;
        _in_fsr_top = in_fsr_top;
        _in_fsr_inv = 1.0f / (in_fsr_top - in_fsr_bot);
    }

    float interpolate(uint16_t x) {
        static_assert(1 < N && N < INT16_MAX, "Filter length must be in this range");
        constexpr auto n_lut_intervals = size_t{N - 1};
        size_t lut_index;
        float partial_intervals;
        if (x > _in_fsr_bot) {
            if (x < _in_fsr_top) {
                auto n = static_cast<int32_t>(n_lut_intervals) * (x - _in_fsr_bot);
                auto d = _in_fsr_top - _in_fsr_bot;
                auto quot_rem = div(n, d);
                partial_intervals = _in_fsr_inv * static_cast<float>(quot_rem.rem);
                lut_index = static_cast<size_t>(quot_rem.quot);
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

protected:
    const std::array<float, N> _lut;
    uint16_t _in_fsr_bot;
    uint16_t _in_fsr_top;
    float _in_fsr_inv;
};


/** @brief Piecewise linear interpolation of look-up-table (LUT) values.
 *
 * LUT values representing function values starting with y(x=in_fsr_bot)
 * and ending with y(x=in_fsr_top).
 *
 * y-values of the LUT must correspond to equidistant X-axis points.
 * 
 * Version for uint32_t input value.
 */
template<size_t N>
class EquidistantPWLUInt32
{
public:
    EquidistantPWLUInt32(const std::array<float, N> &lut,
                   uint32_t in_fsr_bot = 0,
                   uint32_t in_fsr_top = 1)
        : _lut{lut}
    {
        set_input_full_scale_range(in_fsr_bot, in_fsr_top);
    }

    void set_input_full_scale_range(uint32_t in_fsr_bot, uint32_t in_fsr_top) {
        assert(in_fsr_top > in_fsr_bot);
        // 32-bit is native and fast int, also avoid unsigned int promotion
        _in_fsr_bot = in_fsr_bot;
        _in_fsr_top = in_fsr_top;
        _in_fsr_inv = 1.0f / (in_fsr_top - in_fsr_bot);
    }

    float interpolate(uint32_t x) {
        static_assert(1 < N && N < INT32_MAX, "Filter length must be in this range");
        constexpr auto n_lut_intervals = size_t{N - 1};
        size_t lut_index;
        float partial_intervals;
        if (x > _in_fsr_bot) {
            if (x < _in_fsr_top) {
                auto n = static_cast<int64_t>(n_lut_intervals) * (x - _in_fsr_bot);
                auto d = _in_fsr_top - _in_fsr_bot;
                auto quot_rem = lldiv(n, d);
                partial_intervals = _in_fsr_inv * static_cast<float>(quot_rem.rem);
                lut_index = static_cast<size_t>(quot_rem.quot);
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

protected:
    const std::array<float, N> _lut;
    uint32_t _in_fsr_bot;
    uint32_t _in_fsr_top;
    float _in_fsr_inv;
};


#endif