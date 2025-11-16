#ifndef KARMA_MATH_UTILS_HPP
#define KARMA_MATH_UTILS_HPP

#include <algorithm>
#include <cmath>

// =============================================================================
// KARMA MATH UTILITIES - Constexpr Math Helpers
// =============================================================================
// Compile-time and runtime mathematical utilities for audio processing.

namespace karma {

/**
 * @brief Clamp value between min and max
 */
template<typename T>
inline constexpr T clamp(T value, T min, T max) noexcept {
    return (value < min) ? min : ((value > max) ? max : value);
}

/**
 * @brief Convert phase (0.0 to 1.0) to sample position
 */
inline constexpr long phase_to_samples(double phase, long total_samples) noexcept {
    return static_cast<long>(phase * static_cast<double>(total_samples));
}

/**
 * @brief Convert samples to phase (0.0 to 1.0)
 */
inline constexpr double samples_to_phase(long samples, long total_samples) noexcept {
    return (total_samples > 0)
        ? (static_cast<double>(samples) / static_cast<double>(total_samples))
        : 0.0;
}

/**
 * @brief Linear interpolation between two values
 */
template<typename T>
inline constexpr T lerp(T a, T b, double t) noexcept {
    return static_cast<T>(a + t * (b - a));
}

/**
 * @brief Normalize value from [in_min, in_max] to [out_min, out_max]
 */
inline constexpr double normalize(
    double value,
    double in_min, double in_max,
    double out_min, double out_max) noexcept
{
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

/**
 * @brief Check if value is within range [min, max] inclusive
 */
template<typename T>
inline constexpr bool in_range(T value, T min, T max) noexcept {
    return value >= min && value <= max;
}

/**
 * @brief Safe division with fallback for zero divisor
 */
inline constexpr double safe_divide(double numerator, double denominator, double fallback = 0.0) noexcept {
    return (denominator != 0.0) ? (numerator / denominator) : fallback;
}

/**
 * @brief Convert milliseconds to samples at given sample rate
 */
inline constexpr long ms_to_samples(double ms, double sample_rate) noexcept {
    return static_cast<long>(ms * sample_rate / 1000.0);
}

/**
 * @brief Convert samples to milliseconds at given sample rate
 */
inline constexpr double samples_to_ms(long samples, double sample_rate) noexcept {
    return safe_divide(static_cast<double>(samples) * 1000.0, sample_rate, 0.0);
}

/**
 * @brief Sign function (-1, 0, or 1)
 */
template<typename T>
inline constexpr int sign(T value) noexcept {
    return (value > T(0)) - (value < T(0));
}

/**
 * @brief Check if two floating point values are approximately equal
 */
inline constexpr bool approx_equal(double a, double b, double epsilon = 1e-9) noexcept {
    return std::abs(a - b) < epsilon;
}

/**
 * @brief Wrap value to range [0, max] (modulo with proper handling of negatives)
 */
inline constexpr long wrap_to_range(long value, long max) noexcept {
    if (value < 0) {
        return (max + 1) + value;
    } else if (value > max) {
        return value - (max + 1);
    } else {
        return value;
    }
}

} // namespace karma

#endif // KARMA_MATH_UTILS_HPP
