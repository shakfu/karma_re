#include "math.h"
#include "stdlib.h"

#include "ext.h"      // max
#include "ext_obex.h" // attributes

#include "ext_buffer.h" // buffer~
#include "z_dsp.h"      // msp

#include "ext_atomic.h"

// =============================================================================
// KARMA NAMESPACE - C++17 Types and Constants
// =============================================================================

namespace karma {

// =============================================================================
// ENUM DEFINITIONS
// =============================================================================

// =============================================================================
// STATE MACHINE ENUMS
// =============================================================================

/**
 * @brief Internal control state machine for precise looper operation
 *
 * This enum manages the detailed internal state transitions that drive the
 * audio processing engine. States are triggered by user actions and manage
 * complex timing-sensitive operations like fade in/out, overdub transitions,
 * and loop boundary handling.
 *
 * State Transition Flow:
 * ZERO -> RECORD_INITIAL_LOOP (first recording)
 * RECORD_INITIAL_LOOP -> PLAY_ON (loop complete)
 * PLAY_ON -> RECORD_ALT (overdub start)
 * RECORD_ALT -> PLAY_ALT (overdub end)
 * Any state -> JUMP (position change)
 * PLAY_ON -> APPEND -> RECORD_ON (extend loop)
 */
enum class control_state_t {
    ZERO = 0,                // Idle state - no loop exists
    RECORD_INITIAL_LOOP = 1, // Recording the first loop
    RECORD_ALT = 2,          // Recording overdub layer
    RECORD_OFF = 3,          // Stopping record with fade out
    PLAY_ALT = 4,            // Playing after overdub
    PLAY_ON = 5,             // Normal playback state
    STOP_ALT = 6,            // Stopping after overdub
    STOP_REGULAR = 7,        // Normal stop with fade out
    JUMP = 8,                // Jump to specific position
    APPEND = 9,              // Append mode preparation
    APPEND_SPECIAL = 10,     // Append during record/overdub
    RECORD_ON = 11           // Non-looped recording (append mode)
};

/**
 * @brief User-facing state representation for interface feedback
 *
 * Simplified state machine that represents what the user sees and understands.
 * Maps to the complex internal control_state_t but provides clear, intuitive
 * state names for UI elements and user feedback.
 */
enum class human_state_t {
    STOP = 0,    // Stopped - no audio output
    PLAY = 1,    // Playing back recorded loop
    RECORD = 2,  // Recording new material
    OVERDUB = 3, // Overdubbing onto existing loop
    APPEND = 4,  // Appending to extend loop length
    INITIAL = 5  // Initial state before first recording
};

enum class switchramp_type_t {
    LINEAR = 0,     // linear
    SINE_IN = 1,    // sine ease in
    CUBIC_IN = 2,   // cubic ease in
    CUBIC_OUT = 3,  // cubic ease out
    EXPO_IN = 4,    // exponential ease in
    EXPO_OUT = 5,   // exponential ease out
    EXPO_IN_OUT = 6 // exponential ease in/out
};

/**
 * @brief Audio interpolation methods for variable-speed playback
 *
 * Different interpolation algorithms provide trade-offs between:
 * - Audio quality (frequency response, aliasing)
 * - CPU performance (computational cost)
 * - Implementation complexity
 *
 * LINEAR: Fastest, moderate quality
 * - Computational cost: 1 multiply + 1 add per sample
 * - Frequency response: -6dB at Nyquist, some aliasing
 * - Best for: Real-time performance, slight speed variations
 * - Implementation: 2-point linear interpolation
 *
 * CUBIC: Better quality, higher cost
 * - Computational cost: ~4x linear (4-point interpolation)
 * - Frequency response: Improved high-frequency preservation
 * - Best for: Musical applications, noticeable speed changes
 * - Implementation: Hermite cubic 4-point 3rd-order (James McCartney/Alex Harker)
 *
 * SPLINE: Highest quality, highest cost
 * - Computational cost: Significantly higher than cubic
 * - Frequency response: Best preservation across spectrum
 * - Best for: Critical listening, large speed variations
 * - Implementation: Catmull-Rom spline 4-point 3rd-order (Paul Breeuwsma/Paul Bourke)
 */
enum class interp_type_t {
    LINEAR = 0, // Linear interpolation (2-point)
    CUBIC = 1,  // Hermite cubic interpolation (4-point 3rd-order)
    SPLINE = 2  // Catmull-Rom spline interpolation (4-point 3rd-order)
};

} // namespace karma

// =============================================================================
// MAX/MSP INTEGRATION - Global Namespace
// =============================================================================

// Use karma namespace types in global scope for backwards compatibility
using karma::control_state_t;
using karma::human_state_t;
using karma::switchramp_type_t;
using karma::interp_type_t;

typedef struct t_karma t_karma;

// ============================================================================
// PUBLIC FUNCTIONS - These are the main Max/MSP object methods
// ============================================================================

// Core object lifecycle
t_max_err karma_syncout_set(t_karma* x, t_object* attr, long argc, t_atom* argv);
void* karma_new(t_symbol* s, short argc, t_atom* argv);
void  karma_free(t_karma* x);

// Main control methods
void karma_float(t_karma* x, double speedfloat);
void karma_stop(t_karma* x);
void karma_play(t_karma* x);
void karma_record(t_karma* x);
void karma_select_start(t_karma* x, double positionstart);
void karma_overdub(t_karma* x, double amplitude);
void karma_select_size(t_karma* x, double duration);
void karma_setloop(t_karma* x, t_symbol* s, short ac, t_atom* av);
void karma_resetloop(t_karma* x);
void karma_jump(t_karma* x, double jumpposition);
void karma_append(t_karma* x);

// Buffer management
t_max_err karma_buf_notify(t_karma* x, t_symbol* s, t_symbol* msg, void* sndr, void* dat);
void karma_assist(t_karma* x, void* b, long m, long a, char* s);
void karma_buf_dblclick(t_karma* x);
void karma_buf_setup(t_karma* x, t_symbol* s);
void karma_buf_modify(t_karma* x, t_buffer_obj* b);
void karma_buf_change(t_karma* x, t_symbol* s, short ac, t_atom* av);

// DSP and timing
void karma_clock_list(t_karma* x);

void karma_dsp64(
    t_karma* x, t_object* dsp64, short* count, double srate, long vecount, long flags);

long karma_multichanneloutputs(t_karma* x, int index);
long karma_inputchanged(t_karma* x, long index, long count);

void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr);

void karma_stereo_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr);

void karma_poly_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr);
