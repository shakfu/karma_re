#include "math.h"
#include "stdlib.h"

#include "ext.h"      // max
#include "ext_obex.h" // attributes

#include "ext_buffer.h" // buffer~
#include "z_dsp.h"      // msp

#include "ext_atomic.h"

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

t_max_err karma_syncout_set(t_karma* x, t_object* attr, long argc, t_atom* argv);

void* karma_new(t_symbol* s, short argc, t_atom* argv);
void  karma_free(t_karma* x);

void karma_float(t_karma* x, double speedfloat);
void karma_stop(t_karma* x);
void karma_play(t_karma* x);
void karma_record(t_karma* x);

void karma_select_start(t_karma* x, double positionstart);

t_max_err karma_buf_notify(t_karma* x, t_symbol* s, t_symbol* msg, void* sndr, void* dat);

void karma_assist(t_karma* x, void* b, long m, long a, char* s);
void karma_buf_dblclick(t_karma* x);

void karma_overdub(t_karma* x, double amplitude);
void karma_select_size(t_karma* x, double duration);
void karma_setloop(t_karma* x, t_symbol* s, short ac, t_atom* av);
void karma_resetloop(t_karma* x);


void karma_buf_setup(t_karma* x, t_symbol* s);
void karma_buf_modify(t_karma* x, t_buffer_obj* b);
void karma_clock_list(t_karma* x);
void karma_buf_change(t_karma* x, t_symbol* s, short ac, t_atom* av);
// void      karma_offset(t_karma *x, long channeloffset);   // <<-- TODO

void karma_jump(t_karma* x, double jumpposition);
void karma_append(t_karma* x);

void karma_dsp64(
    t_karma* x, t_object* dsp64, short* count, double srate, long vecount, long flags);

void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr);

// Helper functions for karma_mono_perform refactoring
void karma_process_state_control(
    t_karma* x, control_state_t* statecontrol, t_bool* record, t_bool* go,
    t_bool* triginit, t_bool* loopdetermine, long* recordfade, char* recfadeflag,
    long* playfade, char* playfadeflag, char* recendmark);

void karma_initialize_perform_vars(
    t_karma* x, double* accuratehead, long* playhead, double* maxhead, t_bool* wrapflag,
    double* jumphead, double* pokesteps, double* snrfade, double* globalramp,
    double* snrramp, switchramp_type_t* snrtype, interp_type_t* interp,
    double* speedfloat, double* o1prev, double* o1dif, double* writeval1);

void karma_handle_direction_change(
    char directionprev, char direction, t_bool record, double globalramp, long frames,
    float* b, long pchans, long recordhead, long* recordfade, char* recfadeflag,
    double* snrfade);

void karma_handle_record_toggle(
    t_bool record, t_bool recordprev, double globalramp, long frames, float* b,
    long pchans, long* recordhead, long* recordfade, char* recfadeflag,
    double accuratehead, char direction, double speed, double* snrfade, t_bool* dirt);

// Helper functions for karma_buf_change_internal refactoring
t_bool karma_validate_buffer(t_karma* x, t_symbol* bufname);

void karma_parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag);

void karma_parse_numeric_arg(t_atom* arg, double* value);

void karma_process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag);

// Helper functions for further karma_mono_perform refactoring
void karma_handle_ipoke_recording(
    float* b, long pchans, long playhead, long* recordhead, double recin1,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, t_bool* dirt);

void karma_handle_recording_fade(
    double globalramp, long* recordfade, char* recfadeflag, t_bool* record,
    t_bool* triginit, char* jumpflag);

void karma_handle_jump_logic(
    double jumphead, double maxhead, long frames, char directionorig,
    double* accuratehead, char* jumpflag, double* snrfade, t_bool record, float* b,
    long pchans, long* recordhead, char direction, double globalramp, long* recordfade,
    char* recfadeflag, t_bool* triginit);

double karma_process_audio_interpolation(
    float* b, long pchans, double accuratehead, interp_type_t interp, t_bool record);

void handle_initial_loop_ipoke_recording(
    float* b, long pchans, long* recordhead, long playhead, double recin1,
    double* pokesteps, double* writeval1, char direction, char directionorig,
    long maxhead, long frames);

void handle_initial_loop_boundary_constraints(
    double* accuratehead, double speed, double srscale, char direction,
    char directionorig, long frames, long maxloop, long minloop, t_bool append,
    t_bool* record, double globalramp, float* b, long pchans, long* recordhead,
    char* recfadeflag, long* recordfade, char* recendmark, t_bool* triginit,
    t_bool* loopdetermine, t_bool* alternateflag, double* maxhead);
