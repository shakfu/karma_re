#ifndef KARMA_BUFFER_UTILS_HPP
#define KARMA_BUFFER_UTILS_HPP

#include <array>

// =============================================================================
// KARMA BUFFER UTILITIES - Index Calculation and Wrapping
// =============================================================================
// Utilities for calculating buffer indices with proper loop wrapping for
// forward and reverse playback modes.

namespace karma {

/**
 * @brief Wrap buffer index to loop boundaries
 *
 * Handles both forward and reverse playback modes with proper wrapping
 * at loop boundaries.
 *
 * @param index Current buffer index
 * @param is_forward_recording True if loop was recorded forward, false if reverse
 * @param max_loop Maximum loop position (inclusive)
 * @param buffer_frames_minus_1 Buffer size in frames minus 1
 * @return Wrapped index within valid loop range
 */
inline constexpr long wrap_buffer_index(
    long index,
    bool is_forward_recording,
    long max_loop,
    long buffer_frames_minus_1) noexcept
{
    if (is_forward_recording) {
        // Forward: wrap between 0 and max_loop
        if (index < 0) {
            return (max_loop + 1) + index;
        } else if (index > max_loop) {
            return index - (max_loop + 1);
        } else {
            return index;
        }
    } else {
        // Reverse: wrap between (buffer_frames - max_loop) and buffer_frames
        const long min_pos = buffer_frames_minus_1 - max_loop;
        if (index < min_pos) {
            return buffer_frames_minus_1 - (min_pos - index);
        } else if (index > buffer_frames_minus_1) {
            return min_pos + (index - buffer_frames_minus_1);
        } else {
            return index;
        }
    }
}

/**
 * @brief Calculate 4-point interpolation indices for interpolated playback
 *
 * Returns indices for w, x, y, z points needed for cubic/spline interpolation:
 * - indx[0]: point at position -1 (w)
 * - indx[1]: point at position 0  (x) - current playhead
 * - indx[2]: point at position +1 (y)
 * - indx[3]: point at position +2 (z)
 *
 * @param playhead Current playhead position
 * @param direction Playback direction (1 = forward, -1 = reverse)
 * @param is_forward_recording True if loop was recorded forward
 * @param max_loop Maximum loop position
 * @param buffer_frames_minus_1 Buffer size in frames minus 1
 * @return Array of 4 wrapped indices [w, x, y, z]
 */
inline std::array<long, 4> calculate_interp_indices(
    long playhead,
    char direction,
    bool is_forward_recording,
    long max_loop,
    long buffer_frames_minus_1) noexcept
{
    std::array<long, 4> indices;

    indices[0] = wrap_buffer_index(
        playhead - direction, is_forward_recording, max_loop, buffer_frames_minus_1);
    indices[1] = playhead;  // Current position (x)
    indices[2] = wrap_buffer_index(
        playhead + direction, is_forward_recording, max_loop, buffer_frames_minus_1);
    indices[3] = wrap_buffer_index(
        indices[2] + direction, is_forward_recording, max_loop, buffer_frames_minus_1);

    return indices;
}

/**
 * @brief Backward-compatible wrapper for kh_interp_index
 *
 * Updates indices via output parameters for compatibility with existing code.
 */
inline void calculate_interp_indices_legacy(
    long playhead,
    long* indx0, long* indx1, long* indx2, long* indx3,
    char direction,
    bool is_forward_recording,
    long max_loop,
    long buffer_frames_minus_1) noexcept
{
    const auto indices = calculate_interp_indices(
        playhead, direction, is_forward_recording, max_loop, buffer_frames_minus_1);

    *indx0 = indices[0];
    *indx1 = indices[1];
    *indx2 = indices[2];
    *indx3 = indices[3];
}

} // namespace karma

#endif // KARMA_BUFFER_UTILS_HPP
