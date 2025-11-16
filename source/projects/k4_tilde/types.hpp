#ifndef KARMA_TYPES_HPP
#define KARMA_TYPES_HPP

#include <optional>
#include <cstddef>

// =============================================================================
// KARMA TYPE ALIASES - Modern C++ Type Definitions
// =============================================================================
// Type aliases for better code clarity and modern C++ idioms.

namespace karma {

// =============================================================================
// Optional Types - Replace sentinel values
// =============================================================================

/**
 * @brief Optional loop point (replaces -1 sentinel value)
 *
 * Use std::nullopt instead of -1 to represent "no value set".
 * Provides type-safe null checking with .has_value() and .value_or().
 */
using OptionalLoopPoint = std::optional<long>;

/**
 * @brief Optional phase value (0.0 to 1.0)
 */
using OptionalPhase = std::optional<double>;

/**
 * @brief Optional sample position
 */
using OptionalPosition = std::optional<long>;

// =============================================================================
// Audio Processing Types
// =============================================================================

using SamplePosition = long;        // Position in samples
using FrameCount = long;            // Number of frames
using ChannelCount = long;          // Number of audio channels
using SampleRate = double;          // Sample rate in Hz
using Phase = double;               // Normalized position (0.0 to 1.0)
using Amplitude = double;           // Amplitude value
using Milliseconds = double;        // Time in milliseconds

// =============================================================================
// Buffer Types
// =============================================================================

using BufferIndex = long;           // Index into buffer
using BufferSize = long;            // Size of buffer in samples/frames

// =============================================================================
// Utility Functions for Optional Types
// =============================================================================

/**
 * @brief Convert optional loop point to value with default
 */
inline constexpr long loop_point_or(OptionalLoopPoint opt, long default_value) noexcept {
    return opt.value_or(default_value);
}

/**
 * @brief Check if loop point is set (not null)
 */
inline constexpr bool has_loop_point(OptionalLoopPoint opt) noexcept {
    return opt.has_value();
}

/**
 * @brief Create loop point from value (-1 becomes nullopt)
 */
inline constexpr OptionalLoopPoint make_loop_point(long value) noexcept {
    return (value == -1) ? std::nullopt : std::make_optional(value);
}

/**
 * @brief Convert loop point to sentinel value (-1 for nullopt)
 */
inline constexpr long loop_point_to_sentinel(OptionalLoopPoint opt) noexcept {
    return opt.value_or(-1);
}

} // namespace karma

#endif // KARMA_TYPES_HPP
