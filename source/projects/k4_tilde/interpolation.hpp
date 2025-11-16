#ifndef KARMA_INTERPOLATION_HPP
#define KARMA_INTERPOLATION_HPP

// =============================================================================
// KARMA INTERPOLATION - Modern C++17 Header-Only Implementation
// =============================================================================
// Pure interpolation functions for audio sample playback.
// These are constexpr-capable, inline functions for optimal performance.

namespace karma {

// =============================================================================
// INTERPOLATION FUNCTIONS
// =============================================================================

/**
 * @brief Linear interpolation between two points
 *
 * @param frac Fractional position between x and y (0.0 to 1.0)
 * @param x First sample value
 * @param y Second sample value
 * @return Interpolated value
 *
 * Computational cost: 1 multiply + 1 add per sample
 * Quality: -6dB at Nyquist, some aliasing
 */
inline constexpr double linear_interp(double frac, double x, double y) noexcept {
    return x + frac * (y - x);
}

/**
 * @brief Hermite cubic interpolation (4-point, 3rd-order)
 *
 * Implementation by James McCartney / Alex Harker
 *
 * @param frac Fractional position (0.0 to 1.0)
 * @param w Sample at position -1
 * @param x Sample at position 0
 * @param y Sample at position +1
 * @param z Sample at position +2
 * @return Interpolated value
 *
 * Computational cost: ~4x linear interpolation
 * Quality: Improved high-frequency preservation
 */
inline constexpr double cubic_interp(
    double frac, double w, double x, double y, double z) noexcept
{
    return ((((0.5 * (z - w) + 1.5 * (x - y)) * frac +
              (w - 2.5 * x + y + y - 0.5 * z)) * frac +
             (0.5 * (y - w))) * frac + x);
}

/**
 * @brief Catmull-Rom spline interpolation (4-point, 3rd-order)
 *
 * Implementation by Paul Breeuwsma / Paul Bourke
 *
 * @param frac Fractional position (0.0 to 1.0)
 * @param w Sample at position -1
 * @param x Sample at position 0
 * @param y Sample at position +1
 * @param z Sample at position +2
 * @return Interpolated value
 *
 * Computational cost: Higher than cubic (uses pow)
 * Quality: Best preservation across spectrum
 */
inline double spline_interp(
    double frac, double w, double x, double y, double z) noexcept
{
    const double f2 = frac * frac;
    const double f3 = f2 * frac;

    return ((-0.5 * w + 1.5 * x - 1.5 * y + 0.5 * z) * f3) +
           ((w - 2.5 * x + y + y - 0.5 * z) * f2) +
           ((-0.5 * w + 0.5 * y) * frac) + x;
}

/**
 * @brief Perform interpolation based on type enum
 *
 * @param type Interpolation type (LINEAR, CUBIC, or SPLINE)
 * @param frac Fractional position
 * @param w, x, y, z Four sample points (w at -1, x at 0, y at +1, z at +2)
 * @return Interpolated value
 */
inline double interpolate(
    interp_type_t type, double frac,
    double w, double x, double y, double z) noexcept
{
    switch (type) {
        case interp_type_t::CUBIC:
            return cubic_interp(frac, w, x, y, z);
        case interp_type_t::SPLINE:
            return spline_interp(frac, w, x, y, z);
        case interp_type_t::LINEAR:
        default:
            return linear_interp(frac, x, y);
    }
}

/**
 * @brief 2-point interpolation (for cases where only 2 samples available)
 */
inline constexpr double interpolate_2point(
    interp_type_t type, double frac, double x, double y) noexcept
{
    // Always use linear for 2-point interpolation
    return linear_interp(frac, x, y);
}

} // namespace karma

#endif // KARMA_INTERPOLATION_HPP
