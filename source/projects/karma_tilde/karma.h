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

// Enum definitions for clearer state management
typedef enum {
    CONTROL_STATE_ZERO = 0,                // zero/idle
    CONTROL_STATE_RECORD_INITIAL_LOOP = 1, // record initial loop
    CONTROL_STATE_RECORD_ALT = 2,          // record alternateflag (into overdub)
    CONTROL_STATE_RECORD_OFF = 3,          // record off regular
    CONTROL_STATE_PLAY_ALT = 4,            // play alternateflag (out of overdub)
    CONTROL_STATE_PLAY_ON = 5,             // play on regular
    CONTROL_STATE_STOP_ALT = 6,            // stop alternateflag (after overdub)
    CONTROL_STATE_STOP_REGULAR = 7,        // stop regular
    CONTROL_STATE_JUMP = 8,                // jump
    CONTROL_STATE_APPEND = 9,              // append
    CONTROL_STATE_APPEND_SPECIAL = 10,     // special case append (into record/overdub)
    CONTROL_STATE_RECORD_ON = 11           // record on regular (non-looped)
} control_state_t;

typedef enum {
    HUMAN_STATE_STOP = 0,    // stop
    HUMAN_STATE_PLAY = 1,    // play
    HUMAN_STATE_RECORD = 2,  // record
    HUMAN_STATE_OVERDUB = 3, // overdub
    HUMAN_STATE_APPEND = 4,  // append
    HUMAN_STATE_INITIAL = 5  // initial
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

typedef enum {
    INTERP_LINEAR = 0, // linear interpolation
    INTERP_CUBIC = 1,  // cubic interpolation
    INTERP_SPLINE = 2  // spline interpolation
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
