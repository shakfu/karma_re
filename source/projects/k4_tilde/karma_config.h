#ifndef KARMA_CONFIG_H
#define KARMA_CONFIG_H

// =============================================================================
// KARMA~ CONFIGURATION FILE
// =============================================================================
// This file contains configuration constants for the karma~ Max/MSP external.
//
// Modern C++17 version using constexpr for compile-time constants.
// Provides type safety and better debugging compared to preprocessor macros.

namespace karma {

// =============================================================================
// AUDIO PROCESSING
// =============================================================================

// Loop and buffer constraints
constexpr long KARMA_MIN_LOOP_SIZE = 4096;           // Minimum loop size in samples
constexpr long KARMA_SPEED_LIMIT_DIVISOR = 1024;     // Speed limiting factor during recording

// =============================================================================
// MULTICHANNEL LIMITS
// =============================================================================

constexpr long KARMA_POLY_PREALLOC_COUNT = 16;       // Default pre-allocation for multichannel arrays
                                                     // (Performance optimization: avoids
                                                     // reallocation for common scenarios)

constexpr long KARMA_ABSOLUTE_CHANNEL_LIMIT = 64;    // Maximum channels supported in any configuration
                                                     // (Memory safety and performance bound)

// =============================================================================
// FADE AND RAMP CONFIGURATION
// =============================================================================

constexpr long KARMA_DEFAULT_FADE_SAMPLES = 256;              // Default fade time in samples
constexpr long KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE = 257;     // Default fade time + 1 sample
constexpr long KARMA_MAX_RAMP_SAMPLES = 2048;                 // Maximum ramp time allowed in samples

// =============================================================================
// USER INTERFACE
// =============================================================================

constexpr long KARMA_DEFAULT_REPORT_TIME_MS = 50;     // Default report interval in milliseconds
constexpr long KARMA_ASSIST_STRING_MAX_LEN = 256;     // Maximum length for assist strings

// =============================================================================
// INTERNAL CONFIGURATION
// =============================================================================

constexpr double KARMA_SENTINEL_VALUE = -999.0;       // Special flag value for internal logic
constexpr long KARMA_MEMORY_ALIGNMENT = 16;           // Byte alignment for allocated arrays
constexpr bool KARMA_USE_FAST_MATH = true;            // Enable fast math optimizations

// =============================================================================
// DEVELOPMENT AND DEBUGGING
// =============================================================================

constexpr bool KARMA_DEBUG_BUFFER_ACCESS = false;     // Enable buffer access debugging
constexpr bool KARMA_DEBUG_STATE_CHANGES = false;     // Enable state change logging
constexpr bool KARMA_DEBUG_INTERPOLATION = false;     // Enable interpolation debugging
constexpr bool KARMA_VALIDATE_CHANNEL_BOUNDS = true;  // Enable channel bounds checking
constexpr bool KARMA_VALIDATE_BUFFER_SIZES = true;    // Enable buffer size validation

} // namespace karma

#endif // KARMA_CONFIG_H