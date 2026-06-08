#include "math.h"
#include "stdlib.h"

#include "ext.h"      // max
#include "ext_obex.h" // attributes

#include "ext_buffer.h" // buffer~
#include "z_dsp.h"      // msp

#include "ext_atomic.h"

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
typedef enum {
    CONTROL_STATE_ZERO = 0,                // Idle state - no loop exists
    CONTROL_STATE_RECORD_INITIAL_LOOP = 1, // Recording the first loop
    CONTROL_STATE_RECORD_ALT = 2,          // Recording overdub layer
    CONTROL_STATE_RECORD_OFF = 3,          // Stopping record with fade out
    CONTROL_STATE_PLAY_ALT = 4,            // Playing after overdub
    CONTROL_STATE_PLAY_ON = 5,             // Normal playback state
    CONTROL_STATE_STOP_ALT = 6,            // Stopping after overdub
    CONTROL_STATE_STOP_REGULAR = 7,        // Normal stop with fade out
    CONTROL_STATE_JUMP = 8,                // Jump to specific position
    CONTROL_STATE_APPEND = 9,              // Append mode preparation
    CONTROL_STATE_APPEND_SPECIAL = 10,     // Append during record/overdub
    CONTROL_STATE_RECORD_ON = 11           // Non-looped recording (append mode)
} control_state_t;

/**
 * @brief User-facing state representation for interface feedback
 *
 * Simplified state machine that represents what the user sees and understands.
 * Maps to the complex internal control_state_t but provides clear, intuitive
 * state names for UI elements and user feedback.
 */
typedef enum {
    HUMAN_STATE_STOP = 0,    // Stopped - no audio output
    HUMAN_STATE_PLAY = 1,    // Playing back recorded loop
    HUMAN_STATE_RECORD = 2,  // Recording new material
    HUMAN_STATE_OVERDUB = 3, // Overdubbing onto existing loop
    HUMAN_STATE_APPEND = 4,  // Appending to extend loop length
    HUMAN_STATE_INITIAL = 5  // Initial state before first recording
} human_state_t;

typedef enum {
    SWITCHRAMP_LINEAR = 0,     // linear
    SWITCHRAMP_SINE_IN = 1,    // sine ease in
    SWITCHRAMP_CUBIC_IN = 2,   // cubic ease in
    SWITCHRAMP_CUBIC_OUT = 3,  // cubic ease out
    SWITCHRAMP_EXPO_IN = 4,    // exponential ease in
    SWITCHRAMP_EXPO_OUT = 5,   // exponential ease out
    SWITCHRAMP_EXPO_IN_OUT = 6 // exponential ease in/out
} switchramp_type_t;

/**
 * @brief Audio interpolation methods for variable-speed playback
 *
 * Different interpolation algorithms provide trade-offs between:
 * - Audio quality (frequency response, aliasing)
 * - CPU performance (computational cost)
 * - Implementation complexity
 *
 * INTERP_LINEAR: Fastest, moderate quality
 * - Computational cost: 1 multiply + 1 add per sample
 * - Frequency response: -6dB at Nyquist, some aliasing
 * - Best for: Real-time performance, slight speed variations
 * - Implementation: 2-point linear interpolation
 *
 * INTERP_CUBIC: Better quality, higher cost
 * - Computational cost: ~4x linear (4-point interpolation)
 * - Frequency response: Improved high-frequency preservation
 * - Best for: Musical applications, noticeable speed changes
 * - Implementation: Hermite cubic 4-point 3rd-order (James McCartney/Alex Harker)
 *
 * INTERP_SPLINE: Highest quality, highest cost
 * - Computational cost: Significantly higher than cubic
 * - Frequency response: Best preservation across spectrum
 * - Best for: Critical listening, large speed variations
 * - Implementation: Catmull-Rom spline 4-point 3rd-order (Paul Breeuwsma/Paul Bourke)
 */
typedef enum {
    INTERP_LINEAR = 0, // Linear interpolation (2-point)
    INTERP_CUBIC = 1,  // Hermite cubic interpolation (4-point 3rd-order)
    INTERP_SPLINE = 2  // Catmull-Rom spline interpolation (4-point 3rd-order)
} interp_type_t;

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
