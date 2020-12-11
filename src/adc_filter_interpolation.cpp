#include "adc_filter_interpolation.hpp"
#if false
/* Piecewise linear interpolation of look-up-table (LUT) values
 * representing function values starting with Y(X=in_fsr_lower)
 * and ending with Y(X=in_fsr_upper). Y-values of the LUT must
 * correspond to equidistant X-axis points.
 */
float EquidistantPiecewiseLinear::pwl(
        const int32_t in_value, const int32_t in_fsr_lower,
        const int32_t in_fsr_upper, const std::array<const float, 32> &lut_y) {
    const int32_t in_fsr = in_fsr_upper - in_fsr_lower;
    const int32_t n_lut_intervals = lut_y.size() - 1;
    assert(in_fsr > 0);
    assert(n_lut_intervals > 0);
    int32_t lut_index;
    float partial_intervals;
    if (in_value > in_fsr_lower) {
        if (in_value < in_fsr_upper) {
        partial_intervals = static_cast<float>(
            n_lut_intervals * (in_value - in_fsr_lower)
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
    float interval_start = lut_y[lut_index];
    float interval_end;
    if (lut_index < n_lut_intervals) {
        interval_end = lut_y[lut_index + 1];
    } else {
        interval_end = interval_start;
    }
    return interval_start + partial_intervals * (interval_end - interval_start);
}
#endif