#ifndef KARMA_CONFIG_H
#define KARMA_CONFIG_H

// =============================================================================
// KARMA~ CONFIGURATION FILE
// =============================================================================
// This file contains all configurable constants for the karma~ Max/MSP external.
// Modify these values to customize behavior without changing core code.

// =============================================================================
// AUDIO PROCESSING CONFIGURATION
// =============================================================================

// Loop and buffer constraints
#define KARMA_MIN_LOOP_SIZE 4096                    // Minimum loop size in samples
#define KARMA_SPEED_LIMIT_DIVISOR 1024              // Speed limiting factor during recording

// =============================================================================
// CHANNEL ARCHITECTURE CONFIGURATION
// =============================================================================

// The karma~ external uses a hybrid channel architecture for performance:
// - Channels 1-4: Individual struct fields (o1prev, o2prev, o3prev, o4prev)
// - Channels 5+:  Dynamically allocated arrays (poly_oprev[], poly_odif[], etc.)
// This design maintains compatibility while supporting arbitrary channel counts.

// NON-CONFIGURABLE ARCHITECTURAL LIMIT
// This constant reflects the number of individual struct fields and CANNOT be changed
// without modifying the t_karma struct definition and related code.
#define KARMA_STRUCT_CHANNEL_COUNT 4                // Fixed number of o1prev/o2prev/o3prev/o4prev
                                                    // struct fields
                                                    // DO NOT MODIFY - Tied to code structure

// CONFIGURABLE MULTICHANNEL LIMITS
#define KARMA_POLY_PREALLOC_COUNT 16                // Default pre-allocation for multichannel arrays
                                                    // (Performance optimization: avoids 
                                                    // reallocation for common scenarios)

#define KARMA_ABSOLUTE_CHANNEL_LIMIT 64             // Maximum channels supported in any configuration
                                                    // (Memory safety and performance bound)

// Fade and ramp configuration
#define KARMA_DEFAULT_FADE_SAMPLES 256              // Default fade time in samples
#define KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE 257     // Default fade time + 1 sample
#define KARMA_MAX_RAMP_SAMPLES 2048                 // Maximum ramp time allowed in samples

// =============================================================================
// USER INTERFACE CONFIGURATION
// =============================================================================

// Timing and reporting
#define KARMA_DEFAULT_REPORT_TIME_MS 50             // Default report interval in milliseconds

// String and display limits
#define KARMA_ASSIST_STRING_MAX_LEN 256             // Maximum length for assist strings

// =============================================================================
// INTERNAL CONFIGURATION
// =============================================================================

// Special values and flags
#define KARMA_SENTINEL_VALUE -999.0                 // Special flag value for internal logic

// Memory allocation alignment (for performance)
#define KARMA_MEMORY_ALIGNMENT 16                   // Byte alignment for allocated arrays

// Performance tuning
#define KARMA_USE_FAST_MATH 1                       // Enable fast math optimizations 
                                                    // (0 = disabled, 1 = enabled)

// =============================================================================
// DEVELOPMENT AND DEBUGGING CONFIGURATION
// =============================================================================

// Debugging options (set to 1 to enable, 0 to disable)
#define KARMA_DEBUG_BUFFER_ACCESS 0                 // Enable buffer access debugging
#define KARMA_DEBUG_STATE_CHANGES 0                 // Enable state change logging
#define KARMA_DEBUG_INTERPOLATION 0                 // Enable interpolation debugging

// Validation options
#define KARMA_VALIDATE_CHANNEL_BOUNDS 1             // Enable channel bounds checking
#define KARMA_VALIDATE_BUFFER_SIZES 1               // Enable buffer size validation

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

// Compile-time validation of configuration values
#if KARMA_ABSOLUTE_CHANNEL_LIMIT > 256
    #error "KARMA_ABSOLUTE_CHANNEL_LIMIT cannot exceed 256 (performance constraint)"
#endif

#if KARMA_MIN_LOOP_SIZE < 64
    #error "KARMA_MIN_LOOP_SIZE must be at least 64 samples"
#endif

#if KARMA_POLY_PREALLOC_COUNT > KARMA_ABSOLUTE_CHANNEL_LIMIT
    #error "KARMA_POLY_PREALLOC_COUNT cannot exceed KARMA_ABSOLUTE_CHANNEL_LIMIT"
#endif

// Validate architectural constraint (this should never change)
#if KARMA_STRUCT_CHANNEL_COUNT != 4
    #error "KARMA_STRUCT_CHANNEL_COUNT must be 4 (matches o1prev/o2prev/o3prev/o4prev struct fields)"
#endif

// =============================================================================
// DERIVED CONFIGURATION VALUES
// =============================================================================

// Calculate derived values from base configuration
#define KARMA_POLY_ARRAY_SIZE (KARMA_ABSOLUTE_CHANNEL_LIMIT * sizeof(double))

// Interpolation buffer size calculation
#define KARMA_INTERP_BUFFER_SIZE (KARMA_ABSOLUTE_CHANNEL_LIMIT * 4)  // 4 points per channel

#endif // KARMA_CONFIG_H