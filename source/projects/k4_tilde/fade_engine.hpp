#ifndef KARMA_FADE_ENGINE_HPP
#define KARMA_FADE_ENGINE_HPP

#include <cmath>

// =============================================================================
// KARMA FADE ENGINE - Modern C++17 Header-Only Implementation
// =============================================================================
// Fade and ramping functions for smooth audio transitions.

namespace karma {

// =============================================================================
// FADE CURVE CALCULATIONS
// =============================================================================

/**
 * @brief Calculate fade value for recording (ipoke) with cosine curve
 *
 * @param amplitude Base amplitude to scale
 * @param fade_up True for fade-in, false for fade-out
 * @param ramp_length Total ramp length in samples
 * @param current_position Current position in fade (0 to ramp_length)
 * @return Faded amplitude value
 */
inline double ease_record(
    double amplitude,
    bool fade_up,
    double ramp_length,
    long current_position) noexcept
{
    // Use PI from Max SDK (z_dsp.h)
    const double fade_pos = static_cast<double>(current_position) / ramp_length;

    if (fade_up) {
        const double phase = (1.0 - fade_pos) * PI;
        return amplitude * (0.5 * (1.0 - std::cos(phase)));
    } else {
        const double phase = fade_pos * PI;
        return amplitude * (0.5 * (1.0 - std::cos(phase)));
    }
}

/**
 * @brief Calculate fade value using various easing curves
 *
 * @param amplitude Base amplitude to scale
 * @param fade_position Normalized fade position (0.0 to 1.0)
 * @param curve_type Type of easing curve to use
 * @return Faded amplitude value
 */
inline double ease_switchramp(
    double amplitude,
    double fade_position,
    switchramp_type_t curve_type) noexcept
{
    // Use PI from Max SDK (z_dsp.h)
    double fade = fade_position;

    switch (curve_type) {
        case switchramp_type_t::LINEAR:
            return amplitude * (1.0 - fade);

        case switchramp_type_t::SINE_IN:
            return amplitude * (1.0 - (std::sin((fade - 1.0) * PI / 2.0) + 1.0));

        case switchramp_type_t::CUBIC_IN:
            return amplitude * (1.0 - (fade * fade * fade));

        case switchramp_type_t::CUBIC_OUT: {
            const double t = fade - 1.0;
            return amplitude * (1.0 - (t * t * t + 1.0));
        }

        case switchramp_type_t::EXPO_IN:
            fade = (fade == 0.0) ? fade : std::pow(2.0, 10.0 * (fade - 1.0));
            return amplitude * (1.0 - fade);

        case switchramp_type_t::EXPO_OUT:
            fade = (fade == 1.0) ? fade : (1.0 - std::pow(2.0, -10.0 * fade));
            return amplitude * (1.0 - fade);

        case switchramp_type_t::EXPO_IN_OUT:
            if (fade > 0.0 && fade < 0.5) {
                return amplitude * (1.0 - (0.5 * std::pow(2.0, (20.0 * fade) - 10.0)));
            } else if (fade < 1.0 && fade > 0.5) {
                return amplitude * (1.0 - (-0.5 * std::pow(2.0, (-20.0 * fade) + 10.0) + 1.0));
            }
            return amplitude;

        default:
            return amplitude * (1.0 - fade);
    }
}

// =============================================================================
// BUFFER FADE OPERATIONS
// =============================================================================

/**
 * @brief Apply cosine fade-out to buffer region
 *
 * Fades out a region of the buffer starting from mark_position.
 * Used when stopping recording or playback.
 *
 * @param buffer_frames Buffer size in frames (frames - 1)
 * @param buffer Audio buffer to modify
 * @param num_channels Number of interleaved channels
 * @param mark_position Starting position for fade
 * @param direction Direction to fade (1 = forward, -1 = reverse)
 * @param ramp_length Fade length in samples
 */
inline void ease_buffer_fadeout(
    long buffer_frames,
    float* buffer,
    long num_channels,
    long mark_position,
    char direction,
    double ramp_length) noexcept
{
    if (ramp_length <= 0) return;

    // Use PI from Max SDK (z_dsp.h)

    for (long i = 0; i < static_cast<long>(ramp_length); ++i) {
        const long fade_pos = mark_position + (direction * i);

        if (fade_pos < 0 || fade_pos > buffer_frames) {
            continue;
        }

        const double fade = 0.5 * (1.0 - std::cos((static_cast<double>(i) / ramp_length) * PI));

        for (long ch = 0; ch < num_channels; ++ch) {
            buffer[(fade_pos * num_channels) + ch] *= static_cast<float>(fade);
        }
    }
}

/**
 * @brief Apply fade to a single buffer position
 *
 * @param position Frame position to fade
 * @param buffer_frames Buffer size in frames (frames - 1)
 * @param buffer Audio buffer to modify
 * @param num_channels Number of interleaved channels
 * @param fade_value Fade multiplier (0.0 to 1.0)
 */
inline void apply_fade_at_position(
    long position,
    long buffer_frames,
    float* buffer,
    long num_channels,
    double fade_value) noexcept
{
    if (position < 0 || position > buffer_frames) {
        return;
    }

    for (long ch = 0; ch < num_channels; ++ch) {
        buffer[(position * num_channels) + ch] *= static_cast<float>(fade_value);
    }
}

/**
 * @brief Apply cosine fade-in to buffer region (crossfade)
 *
 * Applies fades at multiple positions for smooth transitions when
 * writing to buffer at loop boundaries.
 *
 * @param buffer_frames Buffer size in frames (frames - 1)
 * @param buffer Audio buffer to modify
 * @param num_channels Number of interleaved channels
 * @param mark_position1 First fade position
 * @param mark_position2 Second fade position
 * @param direction Direction to fade (1 = forward, -1 = reverse)
 * @param ramp_length Fade length in samples
 */
inline void ease_buffer_fadein(
    long buffer_frames,
    float* buffer,
    long num_channels,
    long mark_position1,
    long mark_position2,
    char direction,
    double ramp_length) noexcept
{
    // Use PI from Max SDK (z_dsp.h)

    for (long i = 0; i < static_cast<long>(ramp_length); ++i) {
        const double fade = 0.5 * (1.0 - std::cos((static_cast<double>(i) / ramp_length) * PI));

        const long fadpos0 = (mark_position1 - direction) - (direction * i);
        const long fadpos1 = (mark_position2 - direction) - (direction * i);
        const long fadpos2 = mark_position2 + (direction * i);

        apply_fade_at_position(fadpos0, buffer_frames, buffer, num_channels, fade);
        apply_fade_at_position(fadpos1, buffer_frames, buffer, num_channels, 1.0 - fade);
        apply_fade_at_position(fadpos2, buffer_frames, buffer, num_channels, fade);
    }
}

} // namespace karma

#endif // KARMA_FADE_ENGINE_HPP
