#ifndef KARMA_CONFIG_H
#define KARMA_CONFIG_H

// =============================================================================
// KARMA~ CONFIGURATION FILE
// =============================================================================
// This file contains configuration constants for the karma~ Max/MSP external.
//
// These CONFIGURABLE CONSTANTS can be overridden at compile time by defining them
// before including this header or via compiler flags (e.g., -DKARMA_MIN_LOOP_SIZE=8192)

// =============================================================================
// AUDIO PROCESSING
// =============================================================================
// These constants can be overridden at compile time for customization.

// Loop and buffer constraints
#ifndef KARMA_MIN_LOOP_SIZE
#define KARMA_MIN_LOOP_SIZE 4096                    // Minimum loop size in samples
#endif

#ifndef KARMA_SPEED_LIMIT_DIVISOR
#define KARMA_SPEED_LIMIT_DIVISOR 1024              // Speed limiting factor during recording
#endif

// =============================================================================
// MULTICHANNEL LIMITS
// =============================================================================

#ifndef KARMA_POLY_PREALLOC_COUNT
#define KARMA_POLY_PREALLOC_COUNT 16                // Default pre-allocation for multichannel arrays
                                                    // (Performance optimization: avoids
                                                    // reallocation for common scenarios)
#endif

#ifndef KARMA_ABSOLUTE_CHANNEL_LIMIT
#define KARMA_ABSOLUTE_CHANNEL_LIMIT 64             // Maximum channels supported in any configuration
                                                    // (Memory safety and performance bound)
#endif

// =============================================================================
// FADE AND RAMP CONFIGURATION
// =============================================================================

#ifndef KARMA_DEFAULT_FADE_SAMPLES
#define KARMA_DEFAULT_FADE_SAMPLES 256              // Default fade time in samples
#endif

#ifndef KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE
#define KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE 257     // Default fade time + 1 sample
#endif

#ifndef KARMA_MAX_RAMP_SAMPLES
#define KARMA_MAX_RAMP_SAMPLES 2048                 // Maximum ramp time allowed in samples
#endif

// =============================================================================
// USER INTERFACE
// =============================================================================

#ifndef KARMA_DEFAULT_REPORT_TIME_MS
#define KARMA_DEFAULT_REPORT_TIME_MS 50             // Default report interval in milliseconds
#endif

#ifndef KARMA_ASSIST_STRING_MAX_LEN
#define KARMA_ASSIST_STRING_MAX_LEN 256             // Maximum length for assist strings
#endif

// =============================================================================
// INTERNAL CONFIGURATION
// =============================================================================

#ifndef KARMA_SENTINEL_VALUE
#define KARMA_SENTINEL_VALUE -999.0                 // Special flag value for internal logic
#endif

#ifndef KARMA_MEMORY_ALIGNMENT
#define KARMA_MEMORY_ALIGNMENT 16                   // Byte alignment for allocated arrays
#endif

#ifndef KARMA_USE_FAST_MATH
#define KARMA_USE_FAST_MATH 1                       // Enable fast math optimizations
                                                    // (0 = disabled, 1 = enabled)
#endif

// =============================================================================
// DEVELOPMENT AND DEBUGGING
// =============================================================================

#ifndef KARMA_DEBUG_BUFFER_ACCESS
#define KARMA_DEBUG_BUFFER_ACCESS 0                 // Enable buffer access debugging
#endif

#ifndef KARMA_DEBUG_STATE_CHANGES
#define KARMA_DEBUG_STATE_CHANGES 0                 // Enable state change logging
#endif

#ifndef KARMA_DEBUG_INTERPOLATION
#define KARMA_DEBUG_INTERPOLATION 0                 // Enable interpolation debugging
#endif

#ifndef KARMA_VALIDATE_CHANNEL_BOUNDS
#define KARMA_VALIDATE_CHANNEL_BOUNDS 1             // Enable channel bounds checking
#endif

#ifndef KARMA_VALIDATE_BUFFER_SIZES
#define KARMA_VALIDATE_BUFFER_SIZES 1               // Enable buffer size validation
#endif


#endif // KARMA_CONFIG_H