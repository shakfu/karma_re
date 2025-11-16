#include "karma.h"

// =============================================================================
// CONFIGURABLE CONSTANTS
// =============================================================================

#include "karma_config.h"

// =============================================================================
// MODERN C++17 HELPER LIBRARIES
// =============================================================================

#include "interpolation.hpp"
#include "fade_engine.hpp"
#include "poly_arrays.hpp"
#include "buffer_utils.hpp"
#include "math_utils.hpp"
#include "state_machine.hpp"
#include "types.hpp"

// Import karma namespace constants for use in this file
using namespace karma;

// Type aliases for better code clarity
using karma::SamplePosition;
using karma::FrameCount;
using karma::ChannelCount;
using karma::Phase;

// =============================================================================
// NON-CONFIGURABLE ARCHITECTURAL CONSTANTS
// =============================================================================
// These constants reflect fundamental architectural limits and CANNOT be changed
// without modifying the t_karma struct definition and related code.

namespace karma {

// The karma~ external uses a hybrid channel architecture for performance:
// - Channels 1-4: Individual struct fields (o1prev, o2prev, o3prev, o4prev)
// - Channels 5+:  Dynamically allocated arrays (poly_oprev[], poly_odif[], etc.)
// This design maintains compatibility while supporting arbitrary channel counts.

constexpr long KARMA_STRUCT_CHANNEL_COUNT = 4;  // Fixed number of o1prev/o2prev/o3prev/o4prev
                                                // struct fields
                                                // DO NOT MODIFY - Tied to code structure

// =============================================================================
// DERIVED CONFIGURATION VALUES
// =============================================================================

// Calculate derived values from base configuration
constexpr size_t KARMA_POLY_ARRAY_SIZE = KARMA_ABSOLUTE_CHANNEL_LIMIT * sizeof(double);

// Interpolation buffer size calculation
constexpr long KARMA_INTERP_BUFFER_SIZE = KARMA_ABSOLUTE_CHANNEL_LIMIT * 4;  // 4 points per channel

} // namespace karma

// Import architectural constants for use in this file
using karma::KARMA_STRUCT_CHANNEL_COUNT;
using karma::KARMA_POLY_ARRAY_SIZE;
using karma::KARMA_INTERP_BUFFER_SIZE;

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

// Compile-time validation of configuration values using static_assert (C++17)
static_assert(karma::KARMA_ABSOLUTE_CHANNEL_LIMIT <= 256,
              "KARMA_ABSOLUTE_CHANNEL_LIMIT cannot exceed 256 (performance constraint)");

static_assert(karma::KARMA_MIN_LOOP_SIZE >= 64,
              "KARMA_MIN_LOOP_SIZE must be at least 64 samples");

static_assert(karma::KARMA_POLY_PREALLOC_COUNT <= karma::KARMA_ABSOLUTE_CHANNEL_LIMIT,
              "KARMA_POLY_PREALLOC_COUNT cannot exceed KARMA_ABSOLUTE_CHANNEL_LIMIT");

// Validate architectural constraint (this should never change)
static_assert(karma::KARMA_STRUCT_CHANNEL_COUNT == 4,
              "KARMA_STRUCT_CHANNEL_COUNT must be 4 (matches o1prev/o2prev/o3prev/o4prev struct fields)");

// =============================================================================
// KARMA OBJECT STRUCT
// =============================================================================

// clang-format off
struct t_karma {
    
    t_pxobject      k_ob;

    // Buffer management group
    struct {
        t_buffer_ref *buf;
        t_buffer_ref *buf_temp;      // so that 'set' errors etc do not interupt current buf playback ...
        t_symbol     *bufname;
        t_symbol     *bufname_temp;  // ...
        long   bframes;         // number of buffer frames (number of floats long the buffer is for a single channel)
        long   bchans;          // number of buffer channels (number of floats in a frame, stereo has 2 samples per frame, etc.)
        double bsr;             // buffer samplerate
        double bmsr;            // buffer samplerate in samples-per-millisecond
        long   ochans;          // number of object audio channels (object arg #2: 1 / 2 / 4)
        long   nchans;          // number of channels to actually address (use only channel one if 'ochans' == 1, etc.)
    } buffer;

    // Timing and sample rate group
    struct {
        double  ssr;            // system samplerate
        double  srscale;        // scaling factor: buffer samplerate / system samplerate ("to scale playback speeds appropriately")
        double  vs;             // system vectorsize
        double  vsnorm;         // normalised system vectorsize
        double  bvsnorm;        // normalised buffer vectorsize
        double  playhead;       // play position in samples (raja: "double so that capable of tracking playhead position in floating-point indices")
        double  maxhead;        // maximum playhead position that the recording has gone into the buffer~ in samples  // ditto
        double  jumphead;       // jump position (in terms of phase 0..1 of loop) <<-- of 'loop', not 'buffer~'
        long    recordhead;     // record head position in samples
        double  selstart;       // start position of window ('selection') within loop set by the 'position $1' message sent to object (in phase 0..1)
        double  selection;      // selection length of window ('selection') within loop set by 'window $1' message sent to object (in phase 0..1)
        // REMOVED: selmultiply functionality was planned but never implemented
        // Would have provided loop length multiplication feature via 'multiply' method
        // Decision: Leave unimplemented as no current usage exists in codebase
    } timing;

    // Audio processing group
    struct {
        double  o1prev;         // previous sample value of "osamp1" etc...
        double  o2prev;         // ...
        double  o3prev;
        double  o4prev;
        double  o1dif;          // (o1dif = o1prev - osamp1) etc...
        double  o2dif;          // ...
        double  o3dif;
        double  o4dif;
        double  writeval1;      // values to be written into buffer~...
        double  writeval2;      // ...after ipoke~ interpolation, overdub summing, etc...
        double  writeval3;      // ...
        double  writeval4;
        double  overdubamp;     // overdub amplitude 0..1 set by 'overdub $1' message sent to object
        double  overdubprev;    // a 'current' overdub amount ("for smoothing overdub amp changes")
        interp_type_t interpflag; // playback interpolation
        long   pokesteps;       // number of steps (samples) to keep track of in ipoke~ linear averaging scheme
    } audio;

    // Loop boundary group
    struct {
        long   minloop;         // the minimum point in loop so far that has been requested as start point (in samples), is static value
        long   maxloop;         // the overall loop end recorded so far (in samples), is static value
        long   startloop;       // playback start position (in buffer~) in samples, changes depending on loop points and selection logic
        long   endloop;         // playback end position (in buffer~) in samples, changes depending on loop points and selection logic
        long   initiallow;      // store inital loop low point after 'initial loop' (default -1 causes default phase 0)
        long   initialhigh;     // store inital loop high point after 'initial loop' (default -1 causes default phase 1)
    } loop;

    // Fade and ramp control group
    struct {
        long   recordfade;      // fade counter for recording in samples
        long   playfade;        // fade counter for playback in samples
        long   globalramp;      // general fade time (for both recording and playback) in samples
        long   snrramp;         // switch n ramp time in samples ("generally much shorter than general fade time")
        double  snrfade;        // fade counter for switch n ramp, normalised 0..1 ??
        switchramp_type_t snrtype;    // switch n ramp curve option choice
        char    playfadeflag;   // Playback fade state machine flag:
                                // 0 = no fade, 1 = fade out/stop, 2 = switch fade prep,
                                // 3 = fade complete reset, 4 = append mode fade
        char    recfadeflag;    // Recording fade state machine flag:
                                // 0 = no fade, 1 = fade out, 2 = overdub transition,
                                // 3-4 = transition states, 5 = recording continuation
    } fade;

    // State and control group
    struct {
        control_state_t statecontrol;   // master looper state control (not 'human state')
        human_state_t statehuman;       // master looper state human logic (not 'statecontrol')
        char    recendmark;     // the flag to show that the loop is done recording and to mark the ending of it
        char    directionorig;  // original direction loop was recorded ("if loop was initially recorded in reverse started from end-of-buffer etc")
        char    directionprev;  // previous direction ("marker for directional changes to place where fades need to happen during recording")
        t_bool  stopallowed;    // flag, 'false' if already stopped once (& init)
        t_bool  go;             // execute play ??
        t_bool  record;         // record flag
        t_bool  recordprev;     // previous record flag
        t_bool  loopdetermine;  // flag: "...for when object is in a recording stage that actually determines loop duration..."
        t_bool  alternateflag;  // ("rectoo") ARGH ?? !! flag that selects between different types of engagement for statecontrol ??
        t_bool  append;         // append flag ??
        t_bool  triginit;       // flag to show trigger start of ...stuff... (?)
        t_bool  wrapflag;       // flag to show if a window selection wraps around the buffer~ end / beginning
        t_bool  jumpflag;       // whether jump is 'on' or 'off' ("flag to block jumps from coming too soon" ??)
        t_bool  recordinit;     // initial record (raja: "...determine whether to apply the 'record' message to initial loop recording or not")
        t_bool  initinit;       // initial initialise (raja: "...hack i used to determine whether DSP is turned on for the very first time or not")
        t_bool  initskip;       // is initialising = 0
        t_bool  buf_modified;   // buffer has been modified bool
        t_bool  clockgo;        // activate clock (for list outlet)
    } state;

    double  speedfloat;         // store speed inlet value if float (not signal)

    long    syncoutlet;         // make sync outlet ? (object attribute @syncout, instantiation time only)
    // RESERVED: Buffer offset feature for channel indexing
    // long    boffset;         // Would allow starting from specific buffer channel (default 0)
    //                          // Decision: Not implemented - current multichannel design sufficient

    long    moduloout;          // RESERVED: Modulo playback channel outputs
                                // Would cycle through available output channels
                                // Decision: Not implemented - conflicts with MC signal routing

    long    islooped;           // Global looping enable/disable flag
                                // 0 = looping disabled, 1 = looping enabled (default)
                                // Note: Currently not implemented - would require extensive state machine changes

    long   recordhead;          // record head position in samples
    long   reportlist;          // right list outlet report granularity in ms (!! why is this a long ??)

    short   speedconnect;       // 'count[]' info for 'speed' as signal or float in perform routines

    // Multichannel processing arrays (RAII managed)
    karma::PolyArrays *poly_arrays;  // RAII wrapper for poly channel arrays
    long    input_channels;          // current input channel count for auto-adapting

    void    *messout;           // list outlet pointer
    void    *tclock;            // list timer pointer
};

// Include headers after t_karma struct definition (require complete type)
#include "loop_bounds.hpp"
#include "dsp_utils.hpp"
#include "recording_state.hpp"
#include "playback_dsp.hpp"
#include "recording_dsp.hpp"
#include "perform_utils.hpp"
#include "state_control.hpp"
#include "initial_loop.hpp"

static t_symbol *ps_nothing;
static t_symbol *ps_dummy;
static t_symbol *ps_buffer_modified;
static t_symbol *ps_phase;
static t_symbol *ps_samples;
static t_symbol *ps_milliseconds;
static t_symbol *ps_originalloop;

// Include args_parser.hpp after symbol declarations (requires ps_* symbols)
#include "args_parser.hpp"

static t_class  *karma_class = NULL;

// --------------------------------------------------------------------------------------
// forward declarations of private helper functions

static inline double kh_linear_interp(double f, double x, double y);
static inline double kh_cubic_interp(double f, double w, double x, double y, double z);
static inline double kh_spline_interp(double f, double w, double x, double y, double z);
static inline double kh_ease_record(double y1, char updwn, double globalramp, long playfade);
static inline double kh_ease_switchramp(double y1, double snrfade, switchramp_type_t snrtype);
static inline void kh_ease_bufoff(long framesm1, float *buf, long pchans, long markposition, char direction, double globalramp);
static inline void kh_apply_fade(long pos, long framesm1, float *buf, long pchans, double fade);
static inline void kh_ease_bufon(
    long framesm1, float *buf, long pchans, long markposition1, long markposition2, 
    char direction, double globalramp);

static inline void kh_process_recording_fade_completion(
    char recfadeflag, char *recendmark, t_bool *record, 
    t_bool *triginit, t_bool *jumpflag, t_bool *loopdetermine,
    long *recordfade, char directionorig, long *maxloop, 
    long maxhead, long frames);

static inline void kh_calculate_sync_output(
    double osamp1, double *o1prev, double **out1, char syncoutlet,
    double **outPh, double accuratehead, double minloop, double maxloop,
    char directionorig, long frames, double setloopsize);

static inline void kh_apply_ipoke_interpolation(
    float *b, long pchans, long start_idx, long end_idx,
    double *writeval1, double coeff1, char direction);

static inline void kh_init_buffer_properties(t_karma *x, t_buffer_obj *buf);

static inline void kh_process_recording_cleanup(
    t_karma *x, float *b, double accuratehead, char direction, t_bool use_ease_on, double ease_pos);

static inline void kh_process_forward_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction);

static inline void kh_process_reverse_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction);

static inline void kh_process_forward_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction);

static inline void kh_process_reverse_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction);

static inline void kh_process_loop_boundary(
    t_karma *x, float *b, double *accuratehead, double speed, char direction, 
    long setloopsize, t_bool wrapflag, t_bool jumpflag);

static inline double kh_perform_playback_interpolation(
    double frac, float *b, long interp0, long interp1, 
    long interp2, long interp3, long pchans, 
    interp_type_t interp, t_bool record);

static inline void kh_process_playfade_state(
    char *playfadeflag, t_bool *go, t_bool *triginit, t_bool *jumpflag, 
    t_bool *loopdetermine, long *playfade, double *snrfade, t_bool record);

static inline void kh_process_loop_initialization(
    t_karma *x, float *b, double *accuratehead, char direction,
    long *setloopsize, t_bool *wrapflag, char *recendmark_ptr,
    t_bool triginit, t_bool jumpflag);

static inline void kh_process_initial_loop_creation(
    t_karma *x, float *b, double *accuratehead, char direction, t_bool *triginit_ptr);

static inline long kh_wrap_index(long idx, char directionorig, long maxloop, long framesm1);

static inline void kh_interp_index(
    long playhead, long *indx0, long *indx1, long *indx2, long *indx3,
    char direction, char directionorig, long maxloop, long framesm1);

static inline void kh_setloop_internal(t_karma* x, t_symbol* s, short argc, t_atom* argv);

void kh_process_state_control(
    t_karma* x, control_state_t* statecontrol, t_bool* record, t_bool* go,
    t_bool* triginit, t_bool* loopdetermine, long* recordfade, char* recfadeflag,
    long* playfade, char* playfadeflag, char* recendmark);

static inline void kh_initialize_perform_vars(
    t_karma* x, double* accuratehead, long* playhead, t_bool* wrapflag);

static inline void kh_process_direction_change(
    t_karma *x, float *b, char directionprev, char direction);

static inline void kh_process_record_toggle(
    t_karma *x, float *b, double accuratehead, char direction, double speed, t_bool *dirt);

static inline t_bool kh_validate_buffer(t_karma* x, t_symbol* bufname);

static inline void kh_parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag);

static inline void kh_parse_numeric_arg(t_atom* arg, double* value);

static inline void kh_process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag);

static inline void kh_process_ipoke_recording(
    float* b, long pchans, long playhead, long* recordhead, double recin1,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, t_bool* dirt);

static inline void kh_process_recording_fade(
    double globalramp, long* recordfade, char* recfadeflag, t_bool* record,
    t_bool* triginit, t_bool* jumpflag);

static inline void kh_process_jump_logic(
    t_karma *x, float *b, double *accuratehead, t_bool *jumpflag, char direction);

static inline void kh_process_initial_loop_ipoke_recording(
    float* b, long pchans, long* recordhead, long playhead, double recin1,
    double* pokesteps, double* writeval1, char direction, char directionorig,
    long maxhead, long frames);

static inline void kh_process_initial_loop_boundary_constraints(
    t_karma *x, float *b, double *accuratehead, double speed, char direction);

static inline double kh_calculate_interpolation_fraction_and_osamp(
    double accuratehead, char direction, float* b, long pchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record);

static inline double kh_process_ramps_and_fades(
    double osamp1, double* o1prev, double* o1dif, double* snrfade, long* playfade,
    double globalramp, double snrramp, switchramp_type_t snrtype, char* playfadeflag,
    t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record);

static inline void kh_calculate_stereo_interpolation_and_osamp(
    double accuratehead, char direction, float* b, long pchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record,
    double* osamp1, double* osamp2);

static inline void kh_process_stereo_ramps_and_fades(
    double* osamp1, double* osamp2, double* o1prev, double* o2prev, double* o1dif, double* o2dif,
    double* snrfade, long* playfade, double globalramp, double snrramp, switchramp_type_t snrtype,
    char* playfadeflag, t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record);

static inline void kh_calculate_poly_interpolation_and_osamp(
    double accuratehead, char direction, float* b, long pchans, long nchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record, double* osamp);

static inline void kh_process_poly_ramps_and_fades(
    double* osamp, double* oprev, double* odif, long nchans, double* snrfade, long* playfade,
    double globalramp, double snrramp, switchramp_type_t snrtype, char* playfadeflag,
    t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record);
void kh_process_ipoke_recording_stereo(
    float* b, long pchans, long playhead, long* recordhead, double recin1, double recin2,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, double* writeval2, t_bool* dirt);
static inline void kh_process_initial_loop_ipoke_recording_stereo(
    float* b, long pchans, long* recordhead, long playhead, double recin1, double recin2,
    double* pokesteps, double* writeval1, double* writeval2,
    char direction, char directionorig, long maxhead, long frames);


// --------------------------------------------------------------------------------------


// =============================================================================
// INTERPOLATION WRAPPER FUNCTIONS (delegate to interpolation.hpp)
// =============================================================================

static inline double kh_linear_interp(double f, double x, double y) {
    return karma::linear_interp(f, x, y);
}

static inline double kh_cubic_interp(double f, double w, double x, double y, double z) {
    return karma::cubic_interp(f, w, x, y, z);
}

static inline double kh_spline_interp(double f, double w, double x, double y, double z) {
    return karma::spline_interp(f, w, x, y, z);
}


// =============================================================================
// FADE/RAMP WRAPPER FUNCTIONS (delegate to fade_engine.hpp)
// =============================================================================

static inline double kh_ease_record(double y1, char updwn, double globalramp, long playfade)
{
    return karma::ease_record(y1, updwn != 0, globalramp, playfade);
}

static inline double kh_ease_switchramp(double y1, double snrfade, switchramp_type_t snrtype)
{
    return karma::ease_switchramp(y1, snrfade, snrtype);
}

static inline void kh_ease_bufoff(long framesm1, float *buf, long pchans,
                                  long markposition, char direction, double globalramp)
{
    karma::ease_buffer_fadeout(framesm1, buf, pchans, markposition, direction, globalramp);
}

static inline void kh_apply_fade(long pos, long framesm1, float *buf, long pchans, double fade)
{
    karma::apply_fade_at_position(pos, framesm1, buf, pchans, fade);
}


static inline void kh_ease_bufon(
    long framesm1, float *buf, long pchans, long markposition1, long markposition2,
    char direction, double globalramp)
{
    karma::ease_buffer_fadein(framesm1, buf, pchans, markposition1, markposition2,
                              direction, globalramp);
}

// Helper function to handle recording fade completion logic
static inline void kh_process_recording_fade_completion(
    char recfadeflag, char *recendmark, t_bool *record,
    t_bool *triginit, t_bool *jumpflag, t_bool *loopdetermine,
    long *recordfade, char directionorig, long *maxloop,
    long maxhead, long frames)
{
    karma::process_recording_fade_completion(recfadeflag, recendmark, record, triginit, jumpflag,
                                             loopdetermine, recordfade, directionorig, maxloop, maxhead, frames);
}

// Helper function to calculate sync outlet output
static inline void kh_calculate_sync_output(
    double osamp1, double *o1prev, double **out1, char syncoutlet,
    double **outPh, double accuratehead, double minloop, double maxloop,
    char directionorig, long frames, double setloopsize)
{
    karma::calculate_sync_output(osamp1, o1prev, out1, syncoutlet, outPh,
                                   accuratehead, minloop, maxloop, directionorig, frames, setloopsize);
}

// Helper function to apply iPoke interpolation over a range
static inline void kh_apply_ipoke_interpolation(
    float *b, long pchans, long start_idx, long end_idx,
    double *writeval1, double coeff1, char direction)
{
    karma::apply_ipoke_interpolation(b, pchans, start_idx, end_idx, writeval1, coeff1, direction);
}

// Helper function to initialize buffer properties
static inline void kh_init_buffer_properties(t_karma *x, t_buffer_obj *buf) {
    karma::init_buffer_properties(x, buf);
}

// Helper function to handle recording state cleanup after boundary adjustments
static inline void kh_process_recording_cleanup(
    t_karma *x, float *b, double accuratehead, char direction, t_bool use_ease_on, double ease_pos)
{
    karma::process_recording_cleanup(x, b, accuratehead, direction, use_ease_on, ease_pos);
}

// Helper function to handle forward direction boundary wrapping for jumpflag
static inline void kh_process_forward_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    karma::process_forward_jump_boundary(x, b, accuratehead, direction);
}

// Helper function to handle reverse direction boundary wrapping for jumpflag
static inline void kh_process_reverse_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    karma::process_reverse_jump_boundary(x, b, accuratehead, direction);
}

// Helper function to handle forward direction boundaries with wrapflag
static inline void kh_process_forward_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    karma::process_forward_wrap_boundary(x, b, accuratehead, direction);
}

// Helper function to handle reverse direction boundaries with wrapflag
static inline void kh_process_reverse_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    karma::process_reverse_wrap_boundary(x, b, accuratehead, direction);
}

static inline void kh_process_loop_boundary(
    t_karma *x, float *b, double *accuratehead, double speed, char direction,
    long setloopsize, t_bool wrapflag, t_bool jumpflag)
{
    karma::process_loop_boundary(x, b, accuratehead, speed, direction, setloopsize, wrapflag, jumpflag);
}

// Helper function to perform playback interpolation
static inline double kh_perform_playback_interpolation(
    double frac, float *b, long interp0, long interp1,
    long interp2, long interp3, long pchans,
    interp_type_t interp, t_bool record)
{
    return karma::perform_playback_interpolation(frac, b, interp0, interp1, interp2, interp3, pchans, interp, record);
}

// Helper function to handle playfade state machine logic
static inline void kh_process_playfade_state(
    char *playfadeflag, t_bool *go, t_bool *triginit, t_bool *jumpflag,
    t_bool *loopdetermine, long *playfade, double *snrfade, t_bool record)
{
    karma::process_playfade_state(playfadeflag, go, triginit, jumpflag, loopdetermine, playfade, snrfade, record);
}

// Helper function to handle loop initialization and calculation
static inline void kh_process_loop_initialization(
    t_karma *x, float *b, double *accuratehead, char direction,
    long *setloopsize, t_bool *wrapflag, char *recendmark_ptr,
    t_bool triginit, t_bool jumpflag)
{
    karma::process_loop_initialization(x, b, accuratehead, direction, setloopsize, wrapflag,
                                      recendmark_ptr, triginit, jumpflag);
}

// Helper function to handle initial loop creation state
static inline void kh_process_initial_loop_creation(
    t_karma *x, float *b, double *accuratehead, char direction, t_bool *triginit_ptr)
{
    karma::process_initial_loop_creation(x, b, accuratehead, direction, triginit_ptr);
}

// interpolation points
// Helper to wrap index for forward or reverse looping
// =============================================================================
// BUFFER INDEX WRAPPER FUNCTIONS (delegate to buffer_utils.hpp)
// =============================================================================

static inline long kh_wrap_index(long idx, char directionorig, long maxloop, long framesm1) {
    return karma::wrap_buffer_index(idx, directionorig >= 0, maxloop, framesm1);
}

static inline void kh_interp_index(
    long playhead, long *indx0, long *indx1, long *indx2, long *indx3,
    char direction, char directionorig, long maxloop, long framesm1)
{
    karma::calculate_interp_indices_legacy(
        playhead, indx0, indx1, indx2, indx3,
        direction, directionorig >= 0, maxloop, framesm1);
}

// Calculate interpolation fraction and perform audio interpolation in one step
static inline double kh_calculate_interpolation_fraction_and_osamp(
    double accuratehead, char direction, float* b, long pchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record)
{
    return karma::calculate_interpolation_fraction_and_osamp(accuratehead, direction, b, pchans, interp,
                                                              directionorig, maxloop, frames, record);
}

// Process ramps and fades for audio output
static inline double kh_process_ramps_and_fades(
    double osamp1, double* o1prev, double* o1dif, double* snrfade, long* playfade,
    double globalramp, double snrramp, switchramp_type_t snrtype, char* playfadeflag,
    t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record)
{
    return karma::process_ramps_and_fades(osamp1, o1prev, o1dif, snrfade, playfade, globalramp, snrramp,
                                          snrtype, playfadeflag, go, triginit, jumpflag, loopdetermine, record);
}

// Calculate stereo interpolation and get both output samples
static inline void kh_calculate_stereo_interpolation_and_osamp(
    double accuratehead, char direction, float* b, long pchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record,
    double* osamp1, double* osamp2)
{
    karma::calculate_stereo_interpolation_and_osamp(accuratehead, direction, b, pchans, interp,
                                                     directionorig, maxloop, frames, record, osamp1, osamp2);
}

// Process ramps and fades for stereo audio output
static inline void kh_process_stereo_ramps_and_fades(
    double* osamp1, double* osamp2, double* o1prev, double* o2prev, double* o1dif, double* o2dif,
    double* snrfade, long* playfade, double globalramp, double snrramp, switchramp_type_t snrtype,
    char* playfadeflag, t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record)
{
    karma::process_stereo_ramps_and_fades(osamp1, osamp2, o1prev, o2prev, o1dif, o2dif, snrfade, playfade,
                                          globalramp, snrramp, snrtype, playfadeflag, go, triginit,
                                          jumpflag, loopdetermine, record);
}

// Calculate multichannel interpolation and get all output samples
static inline void kh_calculate_poly_interpolation_and_osamp(
    double accuratehead, char direction, float* b, long pchans, long nchans, interp_type_t interp,
    char directionorig, long maxloop, long frames, t_bool record, double* osamp)
{
    karma::calculate_poly_interpolation_and_osamp(accuratehead, direction, b, pchans, nchans, interp,
                                                   directionorig, maxloop, frames, record, osamp);
}

// Process ramps and fades for multichannel audio output
static inline void kh_process_poly_ramps_and_fades(
    double* osamp, double* oprev, double* odif, long nchans, double* snrfade, long* playfade,
    double globalramp, double snrramp, switchramp_type_t snrtype, char* playfadeflag,
    t_bool* go, t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine, t_bool record)
{
    karma::process_poly_ramps_and_fades(osamp, oprev, odif, nchans, snrfade, playfade, globalramp, snrramp,
                                        snrtype, playfadeflag, go, triginit, jumpflag, loopdetermine, record);
}


void ext_main(void *r)
{
    t_class *c = class_new("k4~", (method)karma_new, (method)karma_free, (long)sizeof(t_karma), 0L, A_GIMME, 0);

    class_addmethod(c, (method)karma_select_start,  "position", A_FLOAT,    0);
    class_addmethod(c, (method)karma_select_size,   "window",   A_FLOAT,    0);
    class_addmethod(c, (method)karma_jump,          "jump",     A_FLOAT,    0);
    class_addmethod(c, (method)karma_stop,          "stop",                 0);
    class_addmethod(c, (method)karma_play,          "play",                 0);
    class_addmethod(c, (method)karma_record,        "record",               0);
    class_addmethod(c, (method)karma_append,        "append",               0);
    class_addmethod(c, (method)karma_resetloop,     "resetloop",            0);
    class_addmethod(c, (method)karma_setloop,       "setloop",  A_GIMME,    0);
    class_addmethod(c, (method)karma_buf_change,    "set",      A_GIMME,    0);
    class_addmethod(c, (method)karma_overdub,       "overdub",  A_FLOAT,    0);     // !! A_GIMME ?? (for amplitude + smooth time)
    class_addmethod(c, (method)karma_float,         "float",    A_FLOAT,    0);
    
    class_addmethod(c, (method)karma_dsp64,         "dsp64",    A_CANT,     0);
    class_addmethod(c, (method)karma_assist,        "assist",   A_CANT,     0);
    class_addmethod(c, (method)karma_buf_dblclick,  "dblclick", A_CANT,     0);
    class_addmethod(c, (method)karma_buf_notify,    "notify",   A_CANT,     0);
    class_addmethod(c, (method)karma_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)karma_inputchanged, "inputchanged", A_CANT, 0);

    CLASS_ATTR_LONG(c, "syncout", 0, t_karma, syncoutlet);
    CLASS_ATTR_ACCESSORS(c, "syncout", (method)NULL, (method)karma_syncout_set);    // custom for using at instantiation
    CLASS_ATTR_LABEL(c, "syncout", 0, "Create audio rate Sync Outlet no/yes 0/1");  // not needed anywhere ?
    CLASS_ATTR_INVISIBLE(c, "syncout", 0);                      // do not expose to user, only callable as instantiation attribute

    CLASS_ATTR_LONG(c, "report", 0, t_karma, reportlist);       // !! change to "reporttime" or "listreport" or "listinterval" ??
    CLASS_ATTR_FILTER_MIN(c, "report", 0);
    CLASS_ATTR_LABEL(c, "report", 0, "Report Time (ms) for data outlet");
    
    CLASS_ATTR_LONG(c, "ramp", 0, t_karma, fade.globalramp);         // !! change this to ms input !!    // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "ramp", 0, KARMA_MAX_RAMP_SAMPLES);
    CLASS_ATTR_LABEL(c, "ramp", 0, "Ramp Time (samples)");
    
    CLASS_ATTR_LONG(c, "snramp", 0, t_karma, fade.snrramp);          // !! change this to ms input !!    // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "snramp", 0, KARMA_MAX_RAMP_SAMPLES);
    CLASS_ATTR_LABEL(c, "snramp", 0, "Switch&Ramp Time (samples)");
    
    CLASS_ATTR_LONG(c, "snrcurv", 0, t_karma, fade.snrtype);         // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "snrcurv", 0, 6);
    CLASS_ATTR_ENUMINDEX(c, "snrcurv", 0, "Linear Sine_In Cubic_In Cubic_Out Exp_In Exp_Out Exp_In_Out");
    CLASS_ATTR_LABEL(c, "snrcurv", 0, "Switch&Ramp Curve");
    
    CLASS_ATTR_LONG(c, "interp", 0, t_karma, audio.interpflag);       // !! change to "playinterp" ??
    CLASS_ATTR_FILTER_CLIP(c, "interp", 0, 2);
    CLASS_ATTR_ENUMINDEX(c, "interp", 0, "Linear Cubic Spline");
    CLASS_ATTR_LABEL(c, "interp", 0, "Playback Interpolation");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    karma_class = c;
    
    ps_nothing = gensym("");
    ps_dummy = gensym("dummy");
    ps_buffer_modified = gensym("buffer_modified");
    
    ps_phase = gensym("phase");
    ps_samples = gensym("samples");
    ps_milliseconds = gensym("milliseconds");
    ps_originalloop = gensym("reset");
}
// clang-format on


void* karma_new(t_symbol* s, short argc, t_atom* argv)
{
    t_karma*  x;
    t_symbol* bufname = 0;
    long      syncoutlet = 0;
    long      chans = 0;
    long      attrstart = attr_args_offset(argc, argv);

    x = (t_karma*)object_alloc(karma_class);
    x->state.initskip = 0;

    // should do better argument checks here
    if (attrstart && argv) {
        bufname = atom_getsym(argv + 0);
        // @arg 0 @name buffer_name @optional 0 @type symbol @digest Name of
        // <o>buffer~</o> to be associated with the <o>karma~</o> instance
        // @description Essential argument: <o>karma~</o> will not operate
        // without an associated <o>buffer~</o> <br /> The associated
        // <o>buffer~</o> determines memory and length (if associating a buffer~
        // of <b>0 ms</b> in size <o>karma~</o> will do nothing) <br /> The
        // associated <o>buffer~</o> can be changed on the fly (see the
        // <m>set</m> message) but one must be present on instantiation <br />
        if (attrstart > 1) {
            chans = atom_getlong(argv + 1);
            if (attrstart > 2) {
                // object_error((t_object *)x, "rodrigo! third arg no longer
                // used! use new @syncout attribute instead!");
                object_warn(
                    (t_object*)x,
                    "too many arguments to karma~, ignoring additional crap");
            }
        }
    /*  } else {
            object_error((t_object *)x, "karma~ will not load without an
       associated buffer~ declaration"); goto zero;
    */  }

    if (x) {
        if (chans <= 1) {
            // one audio channel inlet, one signal speed inlet
            dsp_setup((t_pxobject*)x, 2);
            chans = 1;
        } else if (chans == 2) {
            // two audio channel inlets, one signal speed inlet
            dsp_setup((t_pxobject*)x, 3);
            chans = 2;
        } else {
            // four audio channel inlets, one signal speed inlet
            dsp_setup((t_pxobject*)x, 5);
            chans = 4;
        }

        // Allocate multichannel processing arrays for maximum expected channels
        // Calculate channel allocation count (clamp to limits)
        long requested_chans = chans;
        long poly_maxchans = (chans > KARMA_STRUCT_CHANNEL_COUNT) ?
                             ((chans > KARMA_ABSOLUTE_CHANNEL_LIMIT) ? KARMA_ABSOLUTE_CHANNEL_LIMIT : chans) :
                             KARMA_POLY_PREALLOC_COUNT;

        // Warn if we had to clamp the channel count
        if (chans > KARMA_ABSOLUTE_CHANNEL_LIMIT) {
            object_warn((t_object*)x, "Requested %ld channels, but maximum configured is %d. Using %d channels.",
                       requested_chans, KARMA_ABSOLUTE_CHANNEL_LIMIT, KARMA_ABSOLUTE_CHANNEL_LIMIT);
        }

        // Allocate multichannel processing arrays using RAII wrapper
        x->poly_arrays = new (std::nothrow) karma::PolyArrays(poly_maxchans);
        if (!x->poly_arrays || !x->poly_arrays->is_valid()) {
            object_error((t_object*)x, "Failed to allocate memory for multichannel processing arrays");
            delete x->poly_arrays;
            x->poly_arrays = nullptr;
            object_free((t_object*)x);
            return NULL;
        }

        x->input_channels = chans;  // Initialize input channel count

        x->timing.recordhead = -1;
        x->reportlist = KARMA_DEFAULT_REPORT_TIME_MS;                          // ms
        x->fade.snrramp = x->fade.globalramp = KARMA_DEFAULT_FADE_SAMPLES;  // samps...
        x->fade.playfade = x->fade.recordfade = KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE; // ...
        x->timing.ssr = sys_getsr();
        x->timing.vs = sys_getblksize();
        x->timing.vsnorm = x->timing.vs / x->timing.ssr;

        x->audio.overdubprev = 1.0;
        x->audio.overdubamp = 1.0;
        x->speedfloat = 1.0;
        x->islooped = 1;

        x->fade.snrtype = switchramp_type_t::SINE_IN;
        x->audio.interpflag = interp_type_t::CUBIC;
        x->fade.playfadeflag = 0;
        x->fade.recfadeflag = 0;
        x->state.recordinit = 0;
        x->state.initinit = 0;
        x->state.append = 0;
        x->state.jumpflag = 0;
        x->state.statecontrol = control_state_t::ZERO;
        x->state.statehuman = human_state_t::STOP;
        x->state.stopallowed = 0;
        x->state.go = 0;
        x->state.triginit = 0;
        x->state.directionprev = 0;
        x->state.directionorig = 0;
        x->state.recordprev = 0;
        x->state.record = 0;
        x->state.alternateflag = 0;
        x->state.recendmark = 0;
        x->audio.pokesteps = 0;
        x->state.wrapflag = 0;
        x->state.loopdetermine = 0;
        x->audio.writeval1 = x->audio.writeval2 = x->audio.writeval3 = x->audio.writeval4
            = 0;
        x->timing.maxhead = 0.0;
        x->timing.playhead = 0.0;
        x->loop.initiallow = -1;
        x->loop.initialhigh = -1;
        x->timing.selstart = 0.0;
        x->timing.jumphead = 0.0;
        x->fade.snrfade = 0.0;
        x->audio.o1dif = x->audio.o2dif = x->audio.o3dif = x->audio.o4dif = 0.0;
        x->audio.o1prev = x->audio.o2prev = x->audio.o3prev = x->audio.o4prev = 0.0;

        if (bufname != 0)
            x->buffer.bufname = bufname; // !! setup is in 'karma_buf_setup()' called
                                         // by 'karma_dsp64()'...
        /*      else                        // ...(this means double-clicking
           karma~ does not show buffer~ window until DSP turned on, ho hum)
                    object_error((t_object *)x, "requires an associated buffer~
           declaration");
        */

        x->buffer.ochans = chans;
        // @arg 1 @name num_chans @optional 1 @type int @digest Number of Audio
        // channels
        // @description Default = <b>1 (mono)</b> <br />
        // If <b>1</b>, <o>karma~</o> will operate in mono mode with one input
        // for recording and one output for playback <br /> If <b>2</b>,
        // <o>karma~</o> will operate in stereo mode with two inputs for
        // recording and two outputs for playback <br /> If <b>4</b>,
        // <o>karma~</o> will operate in quad mode with four inputs for
        // recording and four outputs for playback <br />

        x->messout = listout(x); // data
        if (!x->messout) {
            object_error((t_object*)x, "Failed to create list outlet");
            delete x->poly_arrays;
            x->poly_arrays = nullptr;
            object_free((t_object*)x);
            return NULL;
        }

        x->tclock = clock_new((t_object*)x, (method)karma_clock_list);
        if (!x->tclock) {
            object_error((t_object*)x, "Failed to create clock");
            object_free(x->messout);
            delete x->poly_arrays;
            x->poly_arrays = nullptr;
            object_free((t_object*)x);
            return NULL;
        }

        attr_args_process(x, argc, argv);
        syncoutlet = x->syncoutlet; // pre-init

        if (chans <= 1) { // mono
            if (syncoutlet)
                outlet_new(x, "signal"); // last: sync (optional)
            outlet_new(x, "signal");     // first: audio output
        } else if (chans == 2) {         // stereo
            if (syncoutlet)
                outlet_new(x, "signal"); // last: sync (optional)
            outlet_new(x, "signal");     // second: audio output 2
            outlet_new(x, "signal");     // first: audio output 1
        } else {                         // multichannel (4+)
            if (syncoutlet)
                outlet_new(x, "signal"); // last: sync (optional)
            outlet_new(x, "multichannelsignal"); // multichannel audio output
        }

        x->state.initskip = 1;
        x->k_ob.z_misc |= Z_NO_INPLACE;

        // Enable multichannel inlet support for all channel counts
        // This allows the object to receive both single and multichannel patch cords
        x->k_ob.z_misc |= Z_MC_INLETS;
    }

    // zero:
    return (x);
}

void karma_free(t_karma* x)
{
    if (x->state.initskip) {
        dsp_free((t_pxobject*)x);

        // Free multichannel processing arrays (RAII automatically handles cleanup)
        delete x->poly_arrays;
        x->poly_arrays = nullptr;

        object_free(x->buffer.buf);
        object_free(x->buffer.buf_temp);
        object_free(x->tclock);
        object_free(x->messout);
    }
}

void karma_buf_dblclick(t_karma* x) { buffer_view(buffer_ref_getobject(x->buffer.buf)); }


// called by 'karma_dsp64' method
void karma_buf_setup(t_karma* x, t_symbol* s)
{
    t_buffer_obj* buf;
    x->buffer.bufname = s;

    if (!x->buffer.buf)
        x->buffer.buf = buffer_ref_new((t_object*)x, s);
    else
        buffer_ref_set(x->buffer.buf, s);

    buf = buffer_ref_getobject(x->buffer.buf);

    if (buf == NULL) {
        x->buffer.buf = 0;
        // object_error((t_object *)x, "there is no buffer~ named %s",
        // s->s_name);
    } else {
        //  if (buf != NULL) {
        x->state.directionorig = 0;
        x->timing.maxhead = x->timing.playhead = 0.0;
        x->timing.recordhead = -1;
        kh_init_buffer_properties(x, buf);
        x->timing.bvsnorm = x->timing.vsnorm
            * (x->buffer.bsr / (double)x->buffer.bframes);
        x->loop.minloop = x->loop.startloop = 0.0;
        x->loop.maxloop = x->loop.endloop = (x->buffer.bframes - 1); // * ((x->bchans > 1)
                                                                     // ? x->bchans : 1);
        x->timing.selstart = 0.0;
        x->timing.selection = 1.0;
    }
}

// called on buffer modified notification
void karma_buf_modify(t_karma* x, t_buffer_obj* b)
{
    double modbsr, modbmsr;
    long   modchans, modframes;

    if (b) {
        modbsr = buffer_getsamplerate(b);
        modchans = buffer_getchannelcount(b);
        modframes = buffer_getframecount(b);
        modbmsr = buffer_getmillisamplerate(b);

        if (((x->buffer.bchans != modchans) || (x->buffer.bframes != modframes))
            || (x->buffer.bmsr != modbmsr)) {
            x->buffer.bsr = modbsr;
            x->buffer.bmsr = modbmsr;
            x->timing.srscale = modbsr / x->timing.ssr; // x->ssr / modbsr;
            x->buffer.bframes = modframes;
            x->buffer.bchans = modchans;
            x->buffer.nchans = (modchans < x->buffer.ochans) ? modchans
                                                             : x->buffer.ochans; // MIN
            x->loop.minloop = x->loop.startloop = 0.0;
            x->loop.maxloop = x->loop.endloop = (x->buffer.bframes - 1); // * ((modchans >
                                                                         // 1) ? modchans
                                                                         // : 1);
            x->timing.bvsnorm = x->timing.vsnorm * (modbsr / (double)modframes);

            karma_select_size(x, x->timing.selection);
            karma_select_start(x, x->timing.selstart);
            //          karma_select_internal(x, x->timing.selstart, x->timing.selection);

            //          post("buff modify called"); // dev
        }
    }
}


void kh_buf_values_internal(
    t_karma* x, double templow, double temphigh, long loop_points_flag, t_bool caller)
{
    //  t_symbol *loop_points_sym = 0;                      // dev
    t_symbol*     caller_sym = 0;
    t_buffer_obj* buf;
    long          bframesm1;              //, bchanscnt;
                                          //  long bchans;
    double bframesms, bvsnorm, bvsnorm05; // !!
    double low, lowtemp, high, hightemp;
    low = templow;
    high = temphigh;

    if (caller) { // only if called from 'karma_buf_change_internal()'
        buf = buffer_ref_getobject(x->buffer.buf);

        kh_init_buffer_properties(x, buf);

        caller_sym = gensym("set");
    } else {
        caller_sym = gensym("setloop");
    }

    // bchans    = x->bchans;
    bframesm1 = (x->buffer.bframes - 1);
    bframesms = (double)bframesm1 / x->buffer.bmsr; // buffersize in milliseconds
    bvsnorm = x->timing.vsnorm
        * (x->buffer.bsr / (double)x->buffer.bframes); // vectorsize in (double) % 0..1
                                                       // (phase) units of buffer~
    bvsnorm05 = bvsnorm * 0.5;                         // half vectorsize (normalised)
    x->timing.bvsnorm = bvsnorm;

    // by this stage in routine, if LOW < 0., it has not been set and should be
    // set to default (0.) regardless of 'loop_points_flag'
    if (low < 0.)
        low = 0.;

    if (loop_points_flag == 0) { // if PHASE
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1

        // (templow already treated as phase 0..1)
    } else if (loop_points_flag == 1) { // if SAMPLES
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1
        else
            high = high / (double)bframesm1; // normalise samples high 0..1..

        if (low > 0.)
            low = low / (double)bframesm1; // normalise samples low 0..1..
    } else {                               // if MILLISECONDS (default)
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1
        else
            high = high / bframesms; // normalise milliseconds high 0..1..

        if (low > 0.)
            low = low / bframesms; // normalise milliseconds low 0..1..
    }

    // !! treated as normalised 0..1 from here on ... min/max & check & clamp
    // once normalisation has occurred
    lowtemp = low;
    hightemp = high;
    low = MIN(lowtemp, hightemp); // low, high = sort_double(low, high);
    high = MAX(lowtemp, hightemp);

    if (low > 1.) { // already sorted (minmax), so if this is the case we know
                    // we are fucked
        object_warn(
            (t_object*)x,
            "loop minimum cannot be greater than available buffer~ size, "
            "setting to buffer~ size minus vectorsize");
        low = 1. - bvsnorm;
    }
    if (high > 1.) {
        object_warn(
            (t_object*)x,
            "loop maximum cannot be greater than available buffer~ size, "
            "setting to buffer~ size");
        high = 1.;
    }

    // finally check for minimum loop-size ...
    if ((high - low) < bvsnorm) {
        if ((high - low) == 0.) {
            object_warn(
                (t_object*)x, "loop size cannot be zero, ignoring %s command",
                caller_sym);
            return;
        } else {
            object_warn(
                (t_object*)x,
                "loop size cannot be this small, minimum is vectorsize "
                "internally (currently using %.0f samples)",
                x->timing.vs);
            if ((low - bvsnorm05) < 0.) {
                low = 0.;
                high = bvsnorm;
            } else if ((high + bvsnorm05) > 1.) {
                high = 1.;
                low = 1. - bvsnorm;
            } else {
                low = low - bvsnorm05;
                high = high + bvsnorm05;
            }
        }
    }

    low = CLAMP(low, 0., 1.);
    high = CLAMP(high, 0., 1.);

    x->loop.minloop = x->loop.startloop = low * bframesm1;
    x->loop.maxloop = x->loop.endloop = high * bframesm1;

    // update selection
    karma_select_size(x, x->timing.selection);
    karma_select_start(x, x->timing.selstart);
}


void kh_buf_change_internal(
    t_karma* x, t_symbol* s, short argc, t_atom* argv) // " set ..... "
{
    t_bool    callerid = true; // identify caller of 'karma_buf_values_internal()'
    t_symbol* bufname;
    long      loop_points_flag;
    double    templow, temphigh;

    // Get buffer name from first argument (already validated in
    // karma_buf_change)
    bufname = atom_getsym(argv + 0);

    // Validate buffer and set up references
    if (!kh_validate_buffer(x, bufname)) {
        return;
    }

    // Reset player state
    x->state.directionorig = 0;
    x->timing.maxhead = x->timing.playhead = 0.0;
    x->timing.recordhead = -1;

    // Process arguments to extract loop points and settings
    kh_process_argc_args(x, s, argc, argv, &templow, &temphigh, &loop_points_flag);

    // Check for early return flag from ps_originalloop handling
    if (templow == KARMA_SENTINEL_VALUE) {
        return;
    }

    // Apply the buffer values
    kh_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

void karma_buf_change(t_karma* x, t_symbol* s, short ac, t_atom* av) // " set ..... "
{
    t_atom store_av[4];
    short  i, j, a;
    a = ac;

    // if error return...

    if (a <= 0) {
        object_error(
            (t_object*)x,
            "%s message must be followed by argument(s) (it does nothing "
            "alone)",
            s->s_name);
        return;
    }

    if (atom_gettype(av + 0) != A_SYM) {
        object_error(
            (t_object*)x,
            "first argument to %s message must be a symbol (associated buffer~ "
            "name)",
            s->s_name);
        return;
    }

    // ...else pass and defer

    if (a > 4) {

        object_warn(
            (t_object*)x,
            "too many arguments for %s message, truncating to first four args",
            s->s_name);
        a = 4;

        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }

    } else {

        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }

        for (j = i; j < 4; j++) {
            atom_setsym(store_av + j, ps_dummy);
        }
    }

    defer(x, (method)kh_buf_change_internal, s, ac, store_av); // main method
    // kh_buf_change_internal(x, s, ac, store_av);
}

void kh_setloop_internal(
    t_karma* x, t_symbol* s, short argc, t_atom* argv) // " setloop ..... "
{
    t_bool    callerid = false; // identify caller of 'karma_buf_values_internal()'
    t_symbol* loop_points_sym = 0;
    long      loop_points_flag; // specify start/end loop points: 0 = in phase, 1 =
                                // in samples, 2 = in milliseconds (default)
    double templow, temphigh, temphightemp;

    loop_points_flag = 2;
    templow = -1.;
    temphigh = -1.;

 
    if (argc >= 3) {

        if (argc > 3)
            object_warn(
                (t_object*)x,
                "too many arguments for %s message, truncating to first three "
                "args",
                s->s_name);

        if (atom_gettype(argv + 2) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 2);
            if ((loop_points_sym == ps_phase) || (loop_points_sym == gensym("PHASE"))
                || (loop_points_sym == gensym("ph"))) // phase
                loop_points_flag = 0;
            else if (
                (loop_points_sym == ps_samples) || (loop_points_sym == gensym("SAMPLES"))
                || (loop_points_sym == gensym("samps"))) // samps
                loop_points_flag = 1;
            else // ms / anything
                loop_points_flag = 2;
        } else if (atom_gettype(argv + 2) == A_LONG) { // can just be int
            loop_points_flag = atom_getlong(argv + 2);
        } else if (atom_gettype(argv + 2) == A_FLOAT) { // convert if error
                                                        // float
            loop_points_flag = (long)atom_getfloat(argv + 2);
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.3, using milliseconds "
                "for args 1 & 2",
                s->s_name);
            loop_points_flag = 2; // default ms
        }

        loop_points_flag = CLAMP(loop_points_flag, 0, 2);
    }

    if (argc >= 2) {

        if (atom_gettype(argv + 1) == A_FLOAT) {
            temphigh = atom_getfloat(argv + 1);
            if (temphigh < 0.) {
                object_warn(
                    (t_object*)x, "loop maximum cannot be less than 0., resetting");
            } // !! do maximum check in karma_buf_values_internal() !!
        } else if (atom_gettype(argv + 1) == A_LONG) {
            temphigh = (double)atom_getlong(argv + 1);
            if (temphigh < 0.) {
                object_warn(
                    (t_object*)x, "loop maximum cannot be less than 0., resetting");
            } // !! do maximum check in karma_buf_values_internal() !!
        } else if ((atom_gettype(argv + 1) == A_SYM) && (argc < 3)) {
            loop_points_sym = atom_getsym(argv + 1);
            if ((loop_points_sym == ps_phase) || (loop_points_sym == gensym("PHASE"))
                || (loop_points_sym == gensym("ph"))) // phase
                loop_points_flag = 0;
            else if (
                (loop_points_sym == ps_samples) || (loop_points_sym == gensym("SAMPLES"))
                || (loop_points_sym == gensym("samps"))) // samps
                loop_points_flag = 1;
            else if (
                (loop_points_sym == ps_milliseconds) || (loop_points_sym == gensym("MS"))
                || (loop_points_sym == gensym("ms"))) // ms
                loop_points_flag = 2;
            else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.2, setting to "
                    "milliseconds",
                    s->s_name);
                loop_points_flag = 2;
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.2, setting unit to "
                "maximum",
                s->s_name);
        }
    }

    if (argc >= 1) {

        if (atom_gettype(argv + 0) == A_FLOAT) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = atom_getfloat(argv + 0);
                templow = temphightemp;
            } else {
                templow = atom_getfloat(argv + 0);
                if (templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.;
                } // !! do maximum check in karma_buf_values_internal() !!
            }
        } else if (atom_gettype(argv + 0) == A_LONG) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = (double)atom_getlong(argv + 0);
                templow = temphightemp;
            } else {
                templow = (double)atom_getlong(argv + 0);
                if (templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.;
                } // !! do maximum check in karma_buf_values_internal() !!
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.1, resetting loop point",
                s->s_name);
        }
    }
    /*
        // dev
        post("%s message:", s->s_name);
    */
    kh_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

void karma_setloop(t_karma* x, t_symbol* s, short ac, t_atom* av) // " setloop ..... "
{
    t_symbol* reset_sym = 0;
    long      points_flag = 1; // initial low/high points stored as (long)samples
                               // internally
    bool   callerid = false;   // false = called from "setloop"
    double initiallow = (double)x->loop.initiallow;
    double initialhigh = (double)x->loop.initialhigh;

    if (ac == 1) { // " setloop reset " message
        if (atom_gettype(av)
            == A_SYM) { // same as sending " resetloop " message to karma~
            reset_sym = atom_getsym(av);
            if (reset_sym == ps_originalloop) { // if "reset" message argument...
                                                // ...go straight to calling function with
                                                // initial loop variables...
                                                //              if (!x->state.recordinit)
                kh_buf_values_internal(x, initiallow, initialhigh, points_flag, callerid);
                //              else
                //                  return;

            } else {
                object_error(
                    (t_object*)x, "%s does not undertsand message %s, ignoring",
                    s->s_name, reset_sym);
                return;
            }
        } else { // ...else pass onto pre-calling parsing function...
            kh_setloop_internal(x, s, ac, av);
        }
    } else { // ...
             //  defer(x, (method)kh_setloop_internal, s, ac, av);
        kh_setloop_internal(x, s, ac, av);
    }
}

// same as sending " setloop reset " message to karma~
void karma_resetloop(t_karma* x) // " resetloop " message only
{
    long points_flag = 1;    // initial low/high points stored as samples
                             // internally
    bool   callerid = false; // false = called from "resetloop"
    double initiallow = (double)x->loop.initiallow;   // initial low/high points stored
                                                      // as long...
    double initialhigh = (double)x->loop.initialhigh; // ...

    //  if (!x->state.recordinit)
    kh_buf_values_internal(x, initiallow, initialhigh, points_flag, callerid);
    //  else
    //      return;
}

void karma_clock_list(t_karma* x)
{
    t_bool rlgtz = x->reportlist > 0;

    if (rlgtz) // ('reportlist 0' == off, else milliseconds)
    {
        long frames = x->buffer.bframes - 1; // !! no '- 1' ??
        long maxloop = x->loop.maxloop;
        long minloop = x->loop.minloop;
        long setloopsize;

        double bmsr = x->buffer.bmsr;
        double playhead = x->timing.playhead;
        double selection = x->timing.selection;
        double normalisedposition;
        setloopsize = maxloop - minloop;

        float reversestart = ((double)(frames - setloopsize));
        float forwardstart = ((double)minloop); // ??           // (minloop + 1)
        float reverseend = ((double)frames);
        float forwardend = ((
            double)maxloop); // !!           // (maxloop + 1)        // !! only
                             // broken on initial buffersize report ?? !!
        float selectionsize = (selection * ((double)setloopsize)); // (setloopsize
                                                                   // + 1)    //
                                                                   // !! only
                                                                   // broken on
                                                                   // initial
                                                                   // buffersize
                                                                   // report ??
                                                                   // !!

        t_bool directflag = x->state.directionorig < 0; // !! reverse = 1, forward = 0
        t_bool record = x->state.record; // pointless (and actually is 'record' or
                                         // 'overdub')
        t_bool go = x->state.go;         // pointless (and actually this is on whenever
                                         // transport is,...
                                         // ...not stricly just 'play')
        human_state_t statehuman = x->state.statehuman;
        //  ((playhead-(frames-maxloop))/setloopsize) :
        //  ((playhead-startloop)/setloopsize)  // ??
        normalisedposition = CLAMP(
            directflag ? ((playhead - (frames - setloopsize)) / setloopsize)
                       : ((playhead - minloop) / setloopsize),
            0., 1.);

        t_atom datalist[7];                              // !! reverse logics are wrong ??
        atom_setfloat(datalist + 0, normalisedposition); // position float normalised 0..1
        atom_setlong(datalist + 1, go);                  // play flag int
        atom_setlong(datalist + 2, record);              // record flag int
        atom_setfloat(
            datalist + 3,
            (directflag ? reversestart : forwardstart) / bmsr); // start float ms
        atom_setfloat(
            datalist + 4,
            (directflag ? reverseend : forwardend) / bmsr);  // end float ms
        atom_setfloat(datalist + 5, (selectionsize / bmsr)); // window float ms
        atom_setlong(datalist + 6, static_cast<long>(statehuman));  // state flag int

        outlet_list(x->messout, 0L, 7, datalist);
        //      outlet_list(x->messout, gensym("list"), 7, datalist);

        if (sys_getdspstate() && (rlgtz)) { // '&& (x->reportlist > 0)' ??
            clock_delay(x->tclock, x->reportlist);
        }
    }
}

void karma_assist(t_karma* x, void* b, long m, long a, char* s)
{
    long dummy;
    long synclet;
    dummy = a + 1;
    synclet = x->syncoutlet;
    a = (a < x->buffer.ochans) ? 0 : ((a > x->buffer.ochans) ? 2 : 1);

    if (m == ASSIST_INLET) {
        switch (a) {
        case 0:
            if (dummy == 1) {
                if (x->buffer.ochans == 1)
                    strncpy_zero(s, "(signal) Record Input / messages to karma~", KARMA_ASSIST_STRING_MAX_LEN);
                else
                    strncpy_zero(s, "(signal) Record Input 1 / messages to karma~", KARMA_ASSIST_STRING_MAX_LEN);
            } else {
                snprintf_zero(s, KARMA_ASSIST_STRING_MAX_LEN, "(signal) Record Input %ld", dummy);
                // @in 0 @type signal @digest Audio Inlet(s)... (object arg #2)
            }
            break;
        case 1:
            strncpy_zero(s, "(signal/float) Speed Factor (1. == normal speed)", KARMA_ASSIST_STRING_MAX_LEN);
            // @in 1 @type signal_or_float @digest Speed Factor (1. = normal
            // speed, < 1. = slower, > 1. = faster)
            break;
        case 2:
            break;
        }
    } else { // ASSIST_OUTLET
        switch (a) {
        case 0:
            if (x->buffer.ochans == 1)
                strncpy_zero(s, "(signal) Audio Output", KARMA_ASSIST_STRING_MAX_LEN);
            else
                snprintf_zero(s, KARMA_ASSIST_STRING_MAX_LEN, "(signal) Audio Output %ld", dummy);
            // @out 0 @type signal @digest Audio Outlet(s)... (object arg #2)
            break;
        case 1:
            if (synclet)
                strncpy_zero(s, "(signal) Sync Outlet (current position 0..1)", KARMA_ASSIST_STRING_MAX_LEN);
            // @out 1 @type signal @digest if chosen (@syncout 1) Sync Outlet
            // (current position 0..1)
            else
                strncpy_zero(
                    s,
                    "List: current position (float 0..1) play state (int 0/1) "
                    "record state (int 0/1) start position (float ms) end "
                    "position (float ms) window size (float ms) current state "
                    "(int 0=stop 1=play 2=record 3=overdub 4=append 5=initial)",
                    512);
            break;
        case 2:
            strncpy_zero(
                s,
                "List: current position (float 0..1) play state (int 0/1) "
                "record state (int 0/1) start position (float ms) end position "
                "(float ms) window size (float ms) current state (int 0=stop "
                "1=play 2=record 3=overdub 4=append 5=initial)",
                512);
            // @out 2 @type list @digest Data Outlet (current position (float
            // 0..1) play state (int 0/1) record state (int 0/1) start position
            // (float ms) end position (float ms) window size (float ms) current
            // state (int 0=stop 1=play 2=record 3=overdub 4=append 5=initial))
            break;
        }
    }
}

void karma_float(t_karma* x, double speedfloat)
{
    long inlet = proxy_getinlet((t_object*)x);
    long chans = (long)x->buffer.ochans;

    if (inlet == chans) { // if speed inlet
        x->speedfloat = speedfloat;
    }
}

void karma_select_start(
    t_karma* x, double positionstart) // positionstart = "position" float message
{
    long bfrmaesminusone, setloopsize;
    x->timing.selstart = CLAMP(positionstart, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->state.loopdetermine) {
        setloopsize = x->loop.maxloop - x->loop.minloop;

        if (x->state.directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->buffer.bframes - 1;

            x->loop.startloop = CLAMP(
                (bfrmaesminusone - x->loop.maxloop) + (positionstart * setloopsize),
                bfrmaesminusone - x->loop.maxloop, bfrmaesminusone);
            x->loop.endloop = x->loop.startloop + (x->timing.selection * x->loop.maxloop);

            if (x->loop.endloop > bfrmaesminusone) {
                x->loop.endloop = (bfrmaesminusone - setloopsize)
                    + (x->loop.endloop - bfrmaesminusone);
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            x->loop.startloop = CLAMP(
                ((positionstart * setloopsize) + x->loop.minloop), x->loop.minloop,
                x->loop.maxloop); // no need for CLAMP ??
            x->loop.endloop = x->loop.startloop + (x->timing.selection * setloopsize);

            if (x->loop.endloop > x->loop.maxloop) {
                x->loop.endloop = x->loop.endloop - setloopsize;
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

void karma_select_size(t_karma* x, double duration) // duration = "window" float message
{
    long bfrmaesminusone, setloopsize;

    // double minsampsnorm = x->timing.bvsnorm * 0.5;           // half vectorsize
    // samples minimum as normalised value  // !! buffer sr !! x->timing.selection =
    // (duration < 0.0) ? 0.0 : duration; // !! allow zero for rodrigo !!
    x->timing.selection = CLAMP(duration, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->state.loopdetermine) {
        setloopsize = x->loop.maxloop - x->loop.minloop;
        x->loop.endloop = x->loop.startloop + (x->timing.selection * setloopsize);

        if (x->state.directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->buffer.bframes - 1;

            if (x->loop.endloop > bfrmaesminusone) {
                x->loop.endloop = (bfrmaesminusone - setloopsize)
                    + (x->loop.endloop - bfrmaesminusone);
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            if (x->loop.endloop > x->loop.maxloop) {
                x->loop.endloop = x->loop.endloop - setloopsize;
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

void karma_stop(t_karma* x)
{
    if (x->state.initinit) {
        if (x->state.stopallowed) {
            x->state.statecontrol = x->state.alternateflag ? control_state_t::STOP_ALT
                                                           : control_state_t::STOP_REGULAR;
            x->state.append = 0;
            x->state.statehuman = human_state_t::STOP;
            x->state.stopallowed = 0;
        }
    }
}

void karma_play(t_karma* x)
{
    if ((!x->state.go) && (x->state.append)) {
        x->state.statecontrol = control_state_t::APPEND;

        x->fade.snrfade = 0.0; // !! should disable ??
    } else if ((x->state.record) || (x->state.append)) {
        x->state.statecontrol = x->state.alternateflag ? control_state_t::PLAY_ALT
                                                       : control_state_t::RECORD_OFF;
    } else {
        x->state.statecontrol = control_state_t::PLAY_ON;
    }

    x->state.go = 1;
    x->state.statehuman = human_state_t::PLAY;
    x->state.stopallowed = 1;
}


// Helper to clear buffer
t_bool _clear_buffer(t_buffer_obj* buf, long bframes, long rchans)
{
    float* b = buffer_locksamples(buf);
    if (!b)
        return 0;
    for (long i = 0; i < bframes; i++) {
        for (long c = 0; c < rchans; c++) {
            b[i * rchans + c] = 0.0f;
        }
    }
    buffer_setdirty(buf);
    buffer_unlocksamples(buf);
    return 1;
};

void karma_record(t_karma* x)
{
    t_buffer_obj*   buf = buffer_ref_getobject(x->buffer.buf);
    control_state_t sc = control_state_t::ZERO;
    human_state_t   sh = x->state.statehuman;
    t_bool          record = x->state.record;
    t_bool          go = x->state.go;
    t_bool          altflag = x->state.alternateflag;
    t_bool          append = x->state.append;
    t_bool          init = x->state.recordinit;

    x->state.stopallowed = 1;

    if (record) {
        if (altflag) {
            sc = control_state_t::RECORD_ALT;
            sh = human_state_t::OVERDUB;
        } else {
            sc = control_state_t::RECORD_OFF;
            sh = (sh == human_state_t::OVERDUB) ? human_state_t::PLAY : human_state_t::RECORD;
        }
    } else if (append) {
        if (go) {
            if (altflag) {
                sc = control_state_t::RECORD_ALT;
                sh = human_state_t::OVERDUB;
            } else {
                sc = control_state_t::APPEND_SPECIAL;
                sh = human_state_t::APPEND;
            }
        } else {
            sc = control_state_t::RECORD_INITIAL_LOOP;
            sh = human_state_t::INITIAL;
        }
    } else if (!go) {
        init = 1;
        if (buf) {
            long rchans = x->buffer.bchans;
            long bframes = x->buffer.bframes;
            _clear_buffer(buf, bframes, rchans);
        }
        sc = control_state_t::RECORD_INITIAL_LOOP;
        sh = human_state_t::INITIAL;
    } else {
        sc = control_state_t::RECORD_ON;
        sh = human_state_t::OVERDUB;
    }

    x->state.go = 1;
    x->state.recordinit = init;
    x->state.statecontrol = sc;
    x->state.statehuman = sh;
}


void karma_append(t_karma* x)
{
    if (x->state.recordinit) {
        if ((!x->state.append) && (!x->state.loopdetermine)) {
            x->state.append = 1;
            x->loop.maxloop = (x->buffer.bframes - 1);
            x->state.statecontrol = control_state_t::APPEND;
            x->state.statehuman = human_state_t::APPEND;
            x->state.stopallowed = 1;
        } else {
            object_error(
                (t_object*)x,
                "can't append if already appending, or during 'initial-loop', "
                "or if buffer~ is full");
        }
    } else {
        object_error(
            (t_object*)x,
            "warning! no 'append' registered until at least one loop has been "
            "created first");
    }
}

void karma_overdub(t_karma* x, double amplitude)
{
    x->audio.overdubamp = CLAMP(amplitude, 0.0, 1.0);
}


void karma_jump(t_karma* x, double jumpposition)
{
    if (x->state.initinit) {
        if (!((x->state.loopdetermine) && (!x->state.record))) {
            x->state.statecontrol = control_state_t::JUMP;
            x->timing.jumphead = CLAMP(
                jumpposition, 0.,
                1.); // Phase-based positioning (0.0 = start, 1.0 = end of loop)
                     // FUTURE ENHANCEMENT: Could support time-based positioning
                     // by converting ms/samples to phase using current loop length
            //          x->state.statehuman = human_state_t::PLAY;           // no -
            //          'jump' is whatever 'statehuman' currently is (most
            //          likely 'play')
            x->state.stopallowed = 1;
        }
    }
}


t_max_err karma_syncout_set(t_karma* x, t_object* attr, long argc, t_atom* argv)
{
    long syncout = atom_getlong(argv);

    if (!x->state.initskip) {
        if (argc && argv) {
            x->syncoutlet = CLAMP(syncout, 0, 1);
        }
    } else {
        object_warn(
            (t_object*)x,
            "the syncout attribute is only available at instantiation time, "
            "ignoring 'syncout %ld'",
            syncout);
    }

    return 0;
}


t_max_err karma_buf_notify(t_karma* x, t_symbol* s, t_symbol* msg, void* sndr, void* dat)
{
    //  t_symbol *bufnamecheck = (t_symbol *)object_method((t_object *)sndr,
    //  gensym("getname"));

    //  if (bufnamecheck == x->bufname) {   // check...
    if (buffer_ref_exists(x->buffer.buf)) { // this hack does not really work...
        if (msg == ps_buffer_modified)
            x->state.buf_modified = true;                           // set flag
        return buffer_ref_notify(x->buffer.buf, s, msg, sndr, dat); // ...return
    } else {
        return MAX_ERR_NONE;
    }
}

long karma_multichanneloutputs(t_karma* x, int index)
{
    // Outlet arrangement when ochans > 2 (multichannel mode):
    // With sync: outlet 0 = sync (1 channel), outlet 1 = multichannel (ochans channels)
    // Without sync: outlet 0 = multichannel (ochans channels)

    if (x->buffer.ochans > 2) {
        if (x->syncoutlet) {
            // With sync outlet: index 0 = sync (1 ch), index 1 = multichannel (ochans ch)
            if (index == 0) return 1;           // sync outlet
            if (index == 1) return x->buffer.ochans;  // multichannel outlet
        } else {
            // Without sync outlet: index 0 = multichannel (ochans ch)
            if (index == 0) return x->buffer.ochans;  // multichannel outlet
        }
    }

    return 1; // Default for non-MC outlets or other cases
}

long karma_inputchanged(t_karma* x, long index, long count)
{
    // Auto-adapt output channel count based on input changes
    // This is called by the MC signal compiler when input channel counts change

    if (count != x->input_channels) {
        x->input_channels = count;

        // For multichannel mode, we auto-adapt our output to match input count
        if (x->buffer.ochans > 2) {
            x->buffer.ochans = count;
            return true;  // Signal that our output channel count may have changed
        }
    }

    return false;  // No change in output channel count
}

void karma_dsp64(
    t_karma* x, t_object* dsp64, short* count, double srate, long vecount, long flags)
{
    x->timing.ssr = srate;
    x->timing.vs = (double)vecount;
    x->timing.vsnorm = (double)vecount / srate; // x->vs / x->ssr;
    x->state.clockgo = 1;

    if (x->buffer.bufname != 0) {
        if (!x->state.initinit)
            karma_buf_setup(x, x->buffer.bufname); // does 'x->timing.bvsnorm'    // !!
                                                   // this should be defered ??

        // For MC objects, query the actual input channel counts
        if (x->buffer.ochans > 2) {
            // Check how many channels are actually connected to our multichannel inputs
            for (int inlet = 0; inlet < x->buffer.ochans; inlet++) {
                if (count[inlet]) {  // Only check connected inlets
                    long channels = (long)object_method(dsp64, gensym("getnuminputchannels"), x, inlet);
                    // For now, just store the first connected inlet's channel count
                    // A more sophisticated implementation might sum or handle differently
                    if (inlet == 0) {
                        x->input_channels = channels;
                    }
                }
            }
        }

        if (x->buffer.ochans > 2) {
            x->speedconnect = count[x->buffer.ochans];     // speed is last inlet
            object_method(dsp64, gensym("dsp_add64"), x, karma_poly_perform, 0, NULL);
        }
        else if (x->buffer.ochans > 1) {
            x->speedconnect = count[2];     // speed is 3rd inlet
            object_method(dsp64, gensym("dsp_add64"), x, karma_stereo_perform, 0, NULL);
        }
        else {
            x->speedconnect = count[1]; // speed is 2nd inlet
            object_method(dsp64, gensym("dsp_add64"), x, karma_mono_perform, 0, NULL);
        }
        if (!x->state.initinit) {
            karma_select_size(x, 1.);
            x->state.initinit = 1;
        } else {
            karma_select_size(x, x->timing.selection);
            karma_select_start(x, x->timing.selstart);
        }
    }
}

/////////////////////////////////// PERFORM ROUTINES

// Helper function: Process state control switch statement
void kh_process_state_control(
    t_karma* x, control_state_t* statecontrol, t_bool* record, t_bool* go,
    t_bool* triginit, t_bool* loopdetermine, long* recordfade, char* recfadeflag,
    long* playfade, char* playfadeflag, char* recendmark)
{
    karma::process_state_control(
        x, statecontrol, record, go, triginit, loopdetermine, recordfade,
        recfadeflag, playfade, playfadeflag, recendmark);
}

// Helper function: Initialize performance variables
static inline void kh_initialize_perform_vars(
    t_karma* x, double* accuratehead, long* playhead, t_bool* wrapflag)
{
    karma::initialize_perform_vars(x, accuratehead, playhead, wrapflag);
}

// Helper function: Handle direction changes
static inline void kh_process_direction_change(t_karma* x, float* b, char directionprev, char direction)
{
    karma::process_direction_change(x, b, directionprev, direction);
}

// Helper function: Handle record on/off transitions
static inline void kh_process_record_toggle(
    t_karma* x, float* b, double accuratehead, char direction, double speed, t_bool* dirt)
{
    karma::process_record_toggle(x, b, accuratehead, direction, speed, dirt);
}

// mono perform

/**
 * @brief Main real-time audio processing function for mono operation
 *
 * This is the core DSP function that processes audio samples for single-channel operation.
 * It implements the complete karma~ looper functionality including:
 * - Real-time recording with optional overdubbing
 * - Playback with variable speed and direction
 * - Cubic/linear interpolation for smooth playback
 * - Complex state machine for loop transitions
 * - Crossfading and ramp processing for artifact-free switching
 *
 * Performance considerations:
 * - Called once per audio vector (typically 64-512 samples)
 * - Must complete within one audio buffer period for real-time operation
 * - Uses helper functions to manage complexity while maintaining performance
 *
 * @param x      The karma~ object instance
 * @param dsp64  Max DSP object (unused in this implementation)
 * @param ins    Input signal vectors [0]=audio, [1]=speed (optional)
 * @param nins   Number of input channels
 * @param outs   Output signal vectors [0]=sync (optional), [1]=audio
 * @param nouts  Number of output channels
 * @param vcount Number of samples to process in this vector
 * @param flgs   DSP flags (unused)
 * @param usr    User data (unused)
 */
void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr)
{
    long syncoutlet = x->syncoutlet;

    double* in1 = ins[0]; // mono in
    double* in2 = ins[1]; // speed (if signal connected)

    double* out1 = outs[0];                   // mono out
    double* outPh = syncoutlet ? outs[1] : 0; // sync (if @syncout 1)

    long  n = vcount;
    short speedinlet = x->speedconnect;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif, pokesteps;
    double speed, speedfloat, osamp1, overdubamp, overdubprev, ovdbdif, selstart,
        selection;
    double o1prev, o1dif, frac, snrfade, globalramp, snrramp, writeval1, coeff1, recin1;
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, dirt,
        wrapflag, triginit;
    char direction, directionprev, directionorig, playfadeflag, recfadeflag, recendmark;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans;
    control_state_t   statecontrol;
    switchramp_type_t snrtype;
    interp_type_t     interp;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop, setloopsize = 0;
    long initiallow, initialhigh;

    t_buffer_obj* buf = buffer_ref_getobject(x->buffer.buf);
    float*        b = buffer_locksamples(buf);

    record = x->state.record;
    recordprev = x->state.recordprev;
    dirt = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt = 1;
    if (x->state.buf_modified) {
        karma_buf_modify(x, buf);
        x->state.buf_modified = false;
    }

    go = x->state.go;
    statecontrol = x->state.statecontrol;
    playfadeflag = x->fade.playfadeflag;
    recfadeflag = x->fade.recfadeflag;
    recordhead = x->timing.recordhead;
    alternateflag = x->state.alternateflag;
    pchans = x->buffer.bchans;
    // srscale = x->timing.srscale;
    frames = x->buffer.bframes;
    triginit = x->state.triginit;
    jumpflag = x->state.jumpflag;
    append = x->state.append;
    directionorig = x->state.directionorig;
    directionprev = x->state.directionprev;
    minloop = x->loop.minloop;
    maxloop = x->loop.maxloop;
    initiallow = x->loop.initiallow;
    initialhigh = x->loop.initialhigh;
    // selection = x->timing.selection;
    loopdetermine = x->state.loopdetermine;
    startloop = x->loop.startloop;
    // selstart = x->timing.selstart;
    endloop = x->loop.endloop;
    recendmark = x->state.recendmark;
    overdubamp = x->audio.overdubprev;
    overdubprev = x->audio.overdubamp;
    ovdbdif = (overdubamp != overdubprev) ? ((overdubprev - overdubamp) / n) : 0.0;
    recordfade = x->fade.recordfade;
    playfade = x->fade.playfade;

    // Initialize performance variables using helper function
    kh_initialize_perform_vars(x, &accuratehead, &playhead, &wrapflag);

    // Access other variables directly from nested structs
    maxhead = x->timing.maxhead;
    // jumphead = x->timing.jumphead;
    pokesteps = x->audio.pokesteps;
    snrfade = x->fade.snrfade;
    globalramp = (double)x->fade.globalramp;
    snrramp = (double)x->fade.snrramp;
    snrtype = x->fade.snrtype;
    interp = x->audio.interpflag;
    speedfloat = x->speedfloat;
    o1prev = x->audio.o1prev;
    o1dif = x->audio.o1dif;
    writeval1 = x->audio.writeval1;

    // Process state control using helper function
    kh_process_state_control(
        x, &statecontrol, &record, &go, &triginit, &loopdetermine, &recordfade,
        &recfadeflag, &playfade, &playfadeflag, &recendmark);

    //  raja notes:
    // 'snrfade = 0.0' triggers switch&ramp (declick play)
    // 'recordhead = -1' triggers ipoke-interp cuts and accompanies buf~ fades
    // (declick record)

    while (n--) {
        recin1 = *in1++;
        speed = speedinlet ? *in2++ : speedfloat; // signal of float ?
        direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

        // Handle direction changes using helper function
        kh_process_direction_change(x, b, directionprev, direction);
        if (directionprev != direction && record && globalramp) {
            recordhead = -1; // Special case handling for recordhead
        }

        // Handle record on/off transitions using helper function
        kh_process_record_toggle(x, b, accuratehead, direction, speed, &dirt);
        recordprev = record;

        if (!loopdetermine) {
            if (go) {
                /*
                calculate_head(directionorig, maxhead, frames, minloop,
                selstart, selection, direction, globalramp, &b, pchans, record,
                jumpflag, jumphead, &maxloop, &setloopsize, &accuratehead,
                &startloop, &endloop, &wrapflag, &recordhead, &snrfade, &append,
                &alternateflag, &recendmark, &triginit, &speedsrscaled,
                &recordfade, &recfadeflag);
                */

                // Handle loop initialization and calculation
                kh_process_loop_initialization(
                    x, b, &accuratehead, direction, &setloopsize, &wrapflag, &recendmark,
                    triginit, jumpflag);
                if (triginit) {
                    recordhead = -1;
                    triginit = 0;
                    if (record && !recendmark) {
                        recordfade = 0;
                        recfadeflag = 0;
                    }
                } else { // jump-based constraints (outside 'window')
                    setloopsize = maxloop - minloop;

                    // Handle loop boundary wrapping and jumping
                    kh_process_loop_boundary(
                        x, b, &accuratehead, speed, direction, setloopsize, wrapflag,
                        jumpflag);

                    // Clear jumpflag if conditions are met
                    if (jumpflag) {
                        if (wrapflag) {
                            if ((accuratehead < endloop) || (accuratehead > startloop))
                                jumpflag = 0;
                        } else {
                            if ((accuratehead < endloop) && (accuratehead > startloop))
                                jumpflag = 0;
                        }
                    }
                }

                /* calculate_head() to here */

                // Calculate interpolation and get output sample
                osamp1 = kh_calculate_interpolation_fraction_and_osamp(
                    accuratehead, direction, b, pchans, interp, directionorig, maxloop, frames, record);

                // Process ramps and fades
                osamp1 = kh_process_ramps_and_fades(
                    osamp1, &o1prev, &o1dif, &snrfade, &playfade, globalramp, snrramp, snrtype,
                    &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine, record);

            } else {
                osamp1 = 0.0;
            }

            kh_calculate_sync_output(
                osamp1, &o1prev, &out1, syncoutlet, &outPh, accuratehead, minloop,
                maxloop, directionorig, frames, setloopsize);

            /*
             ~ipoke - originally by PA Tremblay:
             http://www.pierrealexandretremblay.com/welcome.html (modded to
             allow for 'selection' (window) and 'selstart' (position) to change
             on the fly) raja's razor: simplest answer to everything was: recin1
             = ease_record(recin1 + (b[playhead * pchans] * overdubamp),
             recfadeflag, globalramp, recordfade); ...
             ... placed at the beginning / input of ipoke~ code to apply
             appropriate ramps to oldbuf + newinput (everything all-at-once) ...
             ... allows ipoke~ code to work its sample-specific math / magic
             accurately through the ducking / ramps even at high speed
            */
            if (record) {
                if ((recordfade < globalramp) && (globalramp > 0.0))
                    recin1 = kh_ease_record(
                        recin1 + (((double)b[playhead * pchans]) * overdubamp),
                        recfadeflag, globalramp, recordfade);
                else
                    recin1 += ((double)b[playhead * pchans]) * overdubamp;

                kh_process_ipoke_recording(
                    b, pchans, playhead, &recordhead, recin1, overdubamp, globalramp,
                    recordfade, recfadeflag, &pokesteps, &writeval1, &dirt);
            } // ~ipoke end

            kh_process_recording_fade(
                globalramp, &recordfade, &recfadeflag, &record, &triginit, &jumpflag);
            directionprev = direction;

        } else { // initial loop creation
                 // !! is 'loopdetermine' !!

            if (go) {
                if (triginit) {
                    if (jumpflag) {
                        kh_process_jump_logic(x, b, &accuratehead, &jumpflag, direction);
                    } else if (append) { // append
                        kh_process_initial_loop_creation(
                            x, b, &accuratehead, direction, &triginit);
                        if (!record)
                            goto apned;
                    } else { // trigger start of initial loop creation
                        directionorig = direction;
                        minloop = 0.0;
                        maxloop = frames - 1;
                        maxhead = accuratehead = (direction >= 0) ? minloop : maxloop;
                        alternateflag = 1;
                        recordhead = -1;
                        snrfade = 0.0;
                        triginit = 0;
                    }
                } else {
                apned:
                    kh_process_initial_loop_boundary_constraints(
                        x, b, &accuratehead, speed, direction);
                    // initialhigh = append ? initialhigh : maxhead;   // !! !!
                }

                playhead = trunc(accuratehead);

                if (globalramp) {
                    if (playfade < globalramp) // realtime ramps for play on/off
                    {
                        playfade++;
                        if (playfadeflag) {
                            if (playfade >= globalramp) {
                                if (playfadeflag == 2) {
                                    recendmark = 4;
                                    go = 1;
                                }
                                playfadeflag = 0;
                                switch (recendmark) {
                                case 0:
                                case 1:
                                    go = 0;
                                    break;
                                case 2:
                                case 3:
                                    go = 1;
                                    playfade = 0;
                                    break;
                                case 4:
                                    recendmark = 0;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    if (playfadeflag) {
                        if (playfadeflag == 2) {
                            recendmark = 4;
                            go = 1;
                        }
                        playfadeflag = 0;
                        switch (recendmark) {
                        case 0:
                        case 1:
                            go = 0;
                            break;
                        case 2:
                        case 3:
                            go = 1;
                            break;
                        case 4:
                            recendmark = 0;
                            break;
                        }
                    }
                }
            }

            osamp1 = 0.0;
            kh_calculate_sync_output(
                osamp1, &o1prev, &out1, syncoutlet, &outPh, accuratehead, minloop,
                maxloop, directionorig, frames, setloopsize);

            // ~ipoke - originally by PA Tremblay:
            // http://www.pierrealexandretremblay.com/welcome.html (modded to
            // assume maximum distance recorded into buffer~ as the total
            // length)
            if (record) {
                if ((recordfade < globalramp) && (globalramp > 0.0))
                    recin1 = kh_ease_record(
                        recin1 + ((double)b[playhead * pchans]) * overdubamp, recfadeflag,
                        globalramp, recordfade);
                else
                    recin1 += ((double)b[playhead * pchans]) * overdubamp;

                kh_process_initial_loop_ipoke_recording(
                    b, pchans, &recordhead, playhead, recin1, &pokesteps, &writeval1,
                    direction, directionorig, maxhead, frames); // ~ipoke end
                if (globalramp) // realtime ramps for record on/off
                {
                    if (recordfade < globalramp) {
                        recordfade++;
                        if ((recfadeflag) && (recordfade >= globalramp)) {
                            kh_process_recording_fade_completion(
                                recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                                &loopdetermine, &recordfade, directionorig, &maxloop,
                                maxhead, frames);
                            recfadeflag = 0;
                        }
                    }
                } else {
                    if (recfadeflag) {
                        kh_process_recording_fade_completion(
                            recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                            &loopdetermine, &recordfade, directionorig, &maxloop, maxhead,
                            frames);
                        recfadeflag = 0;
                    }
                } //
                recordhead = playhead;
                dirt = 1;
                // initialhigh = maxloop;
            }
            directionprev = direction;
        }
        if (ovdbdif != 0.0)
            overdubamp = overdubamp + ovdbdif;

        initialhigh = (dirt) ? maxloop : initialhigh; // recordhead ??
    }

    if (dirt) { // notify other buf-related jobs of write
        buffer_setdirty(buf);
    }
    buffer_unlocksamples(buf);

    if (x->state.clockgo) {        // list-outlet stuff
        clock_delay(x->tclock, 0); // why ??
        x->state.clockgo = 0;
    } else if ((!go) || (x->reportlist <= 0)) { // why '!go' ??
        clock_unset(x->tclock);
        x->state.clockgo = 1;
    }

    // Update all state variables back to the main object
    x->audio.o1prev = o1prev;
    x->audio.o1dif = o1dif;
    x->audio.writeval1 = writeval1;
    x->timing.maxhead = maxhead;
    x->audio.pokesteps = pokesteps;
    x->state.wrapflag = wrapflag;
    x->fade.snrfade = snrfade;
    x->timing.playhead = accuratehead;
    x->state.directionorig = directionorig;
    x->state.directionprev = directionprev;
    x->timing.recordhead = recordhead;
    x->state.alternateflag = alternateflag;
    x->fade.recordfade = recordfade;
    x->state.triginit = triginit;
    x->state.jumpflag = jumpflag;
    x->state.go = go;
    x->state.record = record;
    x->state.recordprev = recordprev;
    x->state.statecontrol = statecontrol;
    x->fade.playfadeflag = playfadeflag;
    x->fade.recfadeflag = recfadeflag;
    x->fade.playfade = playfade;
    x->loop.minloop = minloop;
    x->loop.maxloop = maxloop;
    x->loop.initiallow = initiallow;
    x->loop.initialhigh = initialhigh;
    x->state.loopdetermine = loopdetermine;
    x->loop.startloop = startloop;
    x->loop.endloop = endloop;
    x->audio.overdubprev = overdubamp;
    x->state.recendmark = recendmark;
    x->state.append = append;

    return;

zero:
    while (n--) {
        *out1++ = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }

    return;
}

/**
 * @brief Real-time audio processing function for stereo operation
 *
 * Optimized version of the mono perform function for exactly 2 channels.
 * Implements the same looper functionality as mono_perform but with
 * stereo-specific optimizations:
 * - Direct access to o1prev/o2prev struct fields (channels 0-1)
 * - Stereo-optimized interpolation and fade processing
 * - Dual-channel recording and playback
 *
 * @param x      The karma~ object instance
 * @param dsp64  Max DSP object (unused)
 * @param ins    Input vectors [0]=left, [1]=right, [2]=speed (optional)
 * @param nins   Number of input channels
 * @param outs   Output vectors [0]=sync (optional), [1]=left, [2]=right
 * @param nouts  Number of output channels
 * @param vcount Number of samples to process
 * @param flgs   DSP flags (unused)
 * @param usr    User data (unused)
 */
void karma_stereo_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr)
{
    long syncoutlet = x->syncoutlet;

    double* in1 = ins[0]; // left in
    double* in2 = ins[1]; // right in
    double* in3 = ins[2]; // speed (if signal connected)

    double* out1 = outs[0];                   // left out
    double* out2 = outs[1];                   // right out
    double* outPh = syncoutlet ? outs[2] : 0; // sync (if @syncout 1)

    long  n = vcount;
    short speedinlet = x->speedconnect;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif, pokesteps;
    double speed, speedfloat, osamp1, osamp2, overdubamp, overdubprev, ovdbdif, selstart,
        selection;
    double o1prev, o1dif, o2prev, o2dif, frac, snrfade, globalramp, snrramp, writeval1, writeval2, coeff1, coeff2, recin1, recin2;
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, dirt,
        wrapflag, triginit;
    char direction, directionprev, directionorig, playfadeflag, recfadeflag, recendmark;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans;
    control_state_t   statecontrol;
    switchramp_type_t snrtype;
    interp_type_t     interp;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop, setloopsize = 0;
    long initiallow, initialhigh;

    t_buffer_obj* buf = buffer_ref_getobject(x->buffer.buf);
    float*        b = buffer_locksamples(buf);

    record = x->state.record;
    recordprev = x->state.recordprev;
    dirt = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt = 1;
    if (x->state.buf_modified) {
        karma_buf_modify(x, buf);
        x->state.buf_modified = false;
    }

    go = x->state.go;
    statecontrol = x->state.statecontrol;
    playfadeflag = x->fade.playfadeflag;
    recfadeflag = x->fade.recfadeflag;
    recordhead = x->timing.recordhead;
    alternateflag = x->state.alternateflag;
    pchans = x->buffer.bchans;
    // srscale = x->timing.srscale;
    frames = x->buffer.bframes;
    triginit = x->state.triginit;
    jumpflag = x->state.jumpflag;
    append = x->state.append;
    directionorig = x->state.directionorig;
    directionprev = x->state.directionprev;
    minloop = x->loop.minloop;
    maxloop = x->loop.maxloop;
    initiallow = x->loop.initiallow;
    initialhigh = x->loop.initialhigh;
    // selection = x->timing.selection;
    loopdetermine = x->state.loopdetermine;
    startloop = x->loop.startloop;
    // selstart = x->timing.selstart;
    endloop = x->loop.endloop;
    recendmark = x->state.recendmark;
    overdubamp = x->audio.overdubprev;
    overdubprev = x->audio.overdubamp;
    ovdbdif = (overdubamp != overdubprev) ? ((overdubprev - overdubamp) / n) : 0.0;
    recordfade = x->fade.recordfade;
    playfade = x->fade.playfade;

    // Initialize performance variables using helper function
    kh_initialize_perform_vars(x, &accuratehead, &playhead, &wrapflag);

    // Access other variables directly from nested structs
    maxhead = x->timing.maxhead;
    // jumphead = x->timing.jumphead;
    pokesteps = x->audio.pokesteps;
    snrfade = x->fade.snrfade;
    globalramp = (double)x->fade.globalramp;
    snrramp = (double)x->fade.snrramp;
    snrtype = x->fade.snrtype;
    interp = x->audio.interpflag;
    speedfloat = x->speedfloat;
    o1prev = x->audio.o1prev;
    o1dif = x->audio.o1dif;
    o2prev = x->audio.o2prev;
    o2dif = x->audio.o2dif;
    writeval1 = x->audio.writeval1;
    writeval2 = x->audio.writeval2;

    // Process state control using helper function
    kh_process_state_control(
        x, &statecontrol, &record, &go, &triginit, &loopdetermine, &recordfade,
        &recfadeflag, &playfade, &playfadeflag, &recendmark);

    //  raja notes:
    // 'snrfade = 0.0' triggers switch&ramp (declick play)
    // 'recordhead = -1' triggers ipoke-interp cuts and accompanies buf~ fades
    // (declick record)

    while (n--) {
        recin1 = *in1++;
        recin2 = *in2++;
        speed = speedinlet ? *in3++ : speedfloat; // signal of float ?
        direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

        // Handle direction changes using helper function
        kh_process_direction_change(x, b, directionprev, direction);
        if (directionprev != direction && record && globalramp) {
            recordhead = -1; // Special case handling for recordhead
        }

        // Handle record on/off transitions using helper function
        kh_process_record_toggle(x, b, accuratehead, direction, speed, &dirt);
        recordprev = record;

        if (!loopdetermine) {
            if (go) {
                // Handle loop initialization and calculation
                kh_process_loop_initialization(
                    x, b, &accuratehead, direction, &setloopsize, &wrapflag, &recendmark,
                    triginit, jumpflag);
                if (triginit) {
                    recordhead = -1;
                    triginit = 0;
                    if (record && !recendmark) {
                        recordfade = 0;
                        recfadeflag = 0;
                    }
                } else { // jump-based constraints (outside 'window')
                    setloopsize = maxloop - minloop;

                    // Handle loop boundary wrapping and jumping
                    kh_process_loop_boundary(
                        x, b, &accuratehead, speed, direction, setloopsize, wrapflag,
                        jumpflag);

                    // Clear jumpflag if conditions are met
                    if (jumpflag) {
                        if (wrapflag) {
                            if ((accuratehead < endloop) || (accuratehead > startloop))
                                jumpflag = 0;
                        } else {
                            if ((accuratehead < endloop) && (accuratehead > startloop))
                                jumpflag = 0;
                        }
                    }
                }

                // Calculate stereo interpolation and get output samples
                kh_calculate_stereo_interpolation_and_osamp(
                    accuratehead, direction, b, pchans, interp, directionorig, maxloop, frames, record,
                    &osamp1, &osamp2);

                // Process stereo ramps and fades
                kh_process_stereo_ramps_and_fades(
                    &osamp1, &osamp2, &o1prev, &o2prev, &o1dif, &o2dif, &snrfade, &playfade,
                    globalramp, snrramp, snrtype, &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine, record);

            } else {
                osamp1 = 0.0;
                osamp2 = 0.0;
            }

            kh_calculate_sync_output(
                osamp1, &o1prev, &out1, syncoutlet, &outPh, accuratehead, minloop,
                maxloop, directionorig, frames, setloopsize);
            *out2++ = osamp2;
            o2prev = osamp2;

            if (record) {
                if ((recordfade < globalramp) && (globalramp > 0.0)) {
                    recin1 = kh_ease_record(
                        recin1 + (((double)b[playhead * pchans]) * overdubamp),
                        recfadeflag, globalramp, recordfade);
                    if (pchans > 1) {
                        recin2 = kh_ease_record(
                            recin2 + (((double)b[playhead * pchans + 1]) * overdubamp),
                            recfadeflag, globalramp, recordfade);
                    } else {
                        recin2 = recin1;
                    }
                } else {
                    recin1 += ((double)b[playhead * pchans]) * overdubamp;
                    if (pchans > 1) {
                        recin2 += ((double)b[playhead * pchans + 1]) * overdubamp;
                    } else {
                        recin2 = recin1;
                    }
                }

                kh_process_ipoke_recording_stereo(
                    b, pchans, playhead, &recordhead, recin1, recin2, overdubamp, globalramp,
                    recordfade, recfadeflag, &pokesteps, &writeval1, &writeval2, &dirt);
            } // ~ipoke end

            kh_process_recording_fade(
                globalramp, &recordfade, &recfadeflag, &record, &triginit, &jumpflag);
            directionprev = direction;

        } else { // initial loop creation
                 // !! is 'loopdetermine' !!

            if (go) {
                if (triginit) {
                    if (jumpflag) {
                        kh_process_jump_logic(x, b, &accuratehead, &jumpflag, direction);
                    } else if (append) { // append
                        kh_process_initial_loop_creation(
                            x, b, &accuratehead, direction, &triginit);
                        if (!record)
                            goto apned;
                    } else { // trigger start of initial loop creation
                        directionorig = direction;
                        minloop = 0.0;
                        maxloop = frames - 1;
                        maxhead = accuratehead = (direction >= 0) ? minloop : maxloop;
                        alternateflag = 1;
                        recordhead = -1;
                        snrfade = 0.0;
                        triginit = 0;
                    }
                } else {
                apned:
                    kh_process_initial_loop_boundary_constraints(
                        x, b, &accuratehead, speed, direction);
                }

                playhead = trunc(accuratehead);

                if (globalramp) {
                    if (playfade < globalramp) // realtime ramps for play on/off
                    {
                        playfade++;
                        if (playfadeflag) {
                            if (playfade >= globalramp) {
                                if (playfadeflag == 2) {
                                    recendmark = 4;
                                    go = 1;
                                }
                                playfadeflag = 0;
                                switch (recendmark) {
                                case 0:
                                case 1:
                                    go = 0;
                                    break;
                                case 2:
                                case 3:
                                    go = 1;
                                    playfade = 0;
                                    break;
                                case 4:
                                    recendmark = 0;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    if (playfadeflag) {
                        if (playfadeflag == 2) {
                            recendmark = 4;
                            go = 1;
                        }
                        playfadeflag = 0;
                        switch (recendmark) {
                        case 0:
                        case 1:
                            go = 0;
                            break;
                        case 2:
                        case 3:
                            go = 1;
                            break;
                        case 4:
                            recendmark = 0;
                            break;
                        }
                    }
                }
            }

            osamp1 = 0.0;
            osamp2 = 0.0;
            kh_calculate_sync_output(
                osamp1, &o1prev, &out1, syncoutlet, &outPh, accuratehead, minloop,
                maxloop, directionorig, frames, setloopsize);
            *out2++ = osamp2;
            o2prev = osamp2;

            if (record) {
                if ((recordfade < globalramp) && (globalramp > 0.0)) {
                    recin1 = kh_ease_record(
                        recin1 + ((double)b[playhead * pchans]) * overdubamp, recfadeflag,
                        globalramp, recordfade);
                    if (pchans > 1) {
                        recin2 = kh_ease_record(
                            recin2 + ((double)b[playhead * pchans + 1]) * overdubamp, recfadeflag,
                            globalramp, recordfade);
                    } else {
                        recin2 = recin1;
                    }
                } else {
                    recin1 += ((double)b[playhead * pchans]) * overdubamp;
                    if (pchans > 1) {
                        recin2 += ((double)b[playhead * pchans + 1]) * overdubamp;
                    } else {
                        recin2 = recin1;
                    }
                }

                kh_process_initial_loop_ipoke_recording_stereo(
                    b, pchans, &recordhead, playhead, recin1, recin2, &pokesteps, &writeval1, &writeval2,
                    direction, directionorig, maxhead, frames); // ~ipoke end
                if (globalramp) // realtime ramps for record on/off
                {
                    if (recordfade < globalramp) {
                        recordfade++;
                        if ((recfadeflag) && (recordfade >= globalramp)) {
                            kh_process_recording_fade_completion(
                                recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                                &loopdetermine, &recordfade, directionorig, &maxloop,
                                maxhead, frames);
                            recfadeflag = 0;
                        }
                    }
                } else {
                    if (recfadeflag) {
                        kh_process_recording_fade_completion(
                            recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                            &loopdetermine, &recordfade, directionorig, &maxloop, maxhead,
                            frames);
                        recfadeflag = 0;
                    }
                } //
                recordhead = playhead;
                dirt = 1;
            }
            directionprev = direction;
        }
        if (ovdbdif != 0.0)
            overdubamp = overdubamp + ovdbdif;

        initialhigh = (dirt) ? maxloop : initialhigh; // recordhead ??
    }

    if (dirt) { // notify other buf-related jobs of write
        buffer_setdirty(buf);
    }
    buffer_unlocksamples(buf);

    if (x->state.clockgo) {        // list-outlet stuff
        clock_delay(x->tclock, 0); // why ??
        x->state.clockgo = 0;
    } else if ((!go) || (x->reportlist <= 0)) { // why '!go' ??
        clock_unset(x->tclock);
        x->state.clockgo = 1;
    }

    // Update all state variables back to the main object
    x->audio.o1prev = o1prev;
    x->audio.o1dif = o1dif;
    x->audio.o2prev = o2prev;
    x->audio.o2dif = o2dif;
    x->audio.writeval1 = writeval1;
    x->audio.writeval2 = writeval2;
    x->timing.maxhead = maxhead;
    x->audio.pokesteps = pokesteps;
    x->state.wrapflag = wrapflag;
    x->fade.snrfade = snrfade;
    x->timing.playhead = accuratehead;
    x->state.directionorig = directionorig;
    x->state.directionprev = directionprev;
    x->timing.recordhead = recordhead;
    x->state.alternateflag = alternateflag;
    x->fade.recordfade = recordfade;
    x->state.triginit = triginit;
    x->state.jumpflag = jumpflag;
    x->state.go = go;
    x->state.record = record;
    x->state.recordprev = recordprev;
    x->state.statecontrol = statecontrol;
    x->fade.playfadeflag = playfadeflag;
    x->fade.recfadeflag = recfadeflag;
    x->fade.playfade = playfade;
    x->loop.minloop = minloop;
    x->loop.maxloop = maxloop;
    x->loop.initiallow = initiallow;
    x->loop.initialhigh = initialhigh;
    x->state.loopdetermine = loopdetermine;
    x->loop.startloop = startloop;
    x->loop.endloop = endloop;
    x->audio.overdubprev = overdubamp;
    x->state.recendmark = recendmark;
    x->state.append = append;

    return;

zero:
    while (n--) {
        *out1++ = 0.0;
        *out2++ = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }

    return;
}

/**
 * @brief Real-time audio processing function for multichannel operation
 *
 * Handles arbitrary channel counts (3+ channels) using the hybrid architecture:
 * - Channels 0-3: Direct access to o1prev-o4prev struct fields for performance
 * - Channels 4+: Dynamic allocation using poly_oprev/poly_odif arrays
 *
 * Key features:
 * - Supports up to KARMA_ABSOLUTE_CHANNEL_LIMIT channels (default: 64)
 * - Runtime memory allocation for channels beyond the first 4
 * - Max/MSP MC (multichannel) signal integration
 * - Automatic channel count adaptation via inputchanged protocol
 *
 * Memory management:
 * - Pre-allocates arrays for KARMA_POLY_PREALLOC_COUNT channels (default: 16)
 * - Reallocates if needed, but avoids malloc/free in perform function
 * - Uses poly_nchans_alloc to track allocated capacity
 *
 * @param x      The karma~ object instance
 * @param dsp64  Max DSP object (unused)
 * @param ins    Input signal vectors (multichannel + optional speed)
 * @param nins   Number of input channels
 * @param outs   Output signal vectors (optional sync + multichannel)
 * @param nouts  Number of output channels
 * @param vcount Number of samples to process
 * @param flgs   DSP flags (unused)
 * @param usr    User data (unused)
 */
void karma_poly_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr)
{
    long syncoutlet = x->syncoutlet;
    long nchans = x->buffer.ochans;

    // Safety check: ensure we don't exceed allocated memory or configuration limits
    if (nchans > x->poly_arrays->max_channels()) {
        nchans = x->poly_arrays->max_channels();
    }
#if KARMA_VALIDATE_CHANNEL_BOUNDS
    if (nchans > KARMA_ABSOLUTE_CHANNEL_LIMIT) {
        error("karma~: Channel count %ld exceeds maximum configured channels (%d)",
              nchans, KARMA_ABSOLUTE_CHANNEL_LIMIT);
        nchans = KARMA_ABSOLUTE_CHANNEL_LIMIT;
    }
#endif

    // MC Signal Routing (per Max MC API docs):
    // - ins[0..nchans-1] are audio inputs, ins[nchans] is speed
    // - numouts tells us total number of output channels across all outlets
    // - if syncoutlet: we have sync outlet (1 ch) + multichannel outlet (nchans ch) = 1+nchans total
    // - if no syncoutlet: we have only multichannel outlet (nchans ch) = nchans total

    double* speed_in = ins[nchans];
    double* sync_out = NULL;
    long multichannel_start_idx = 0;

    if (syncoutlet) {
        // With sync: outs[0] = sync channel, outs[1..nchans] = multichannel channels
        sync_out = outs[0];
        multichannel_start_idx = 1;
    } else {
        // Without sync: outs[0..nchans-1] = multichannel channels
        multichannel_start_idx = 0;
    }

    long  n = vcount;
    short speedinlet = x->speedconnect;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif, pokesteps;
    double speed, speedfloat, overdubamp, overdubprev, ovdbdif, selstart, selection;
    double frac, snrfade, globalramp, snrramp, coeff1, writeval1;
    double* osamp = x->poly_arrays->osamp();
    double* oprev = x->poly_arrays->oprev();
    double* odif = x->poly_arrays->odif();
    double* recin = x->poly_arrays->recin();

    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, dirt,
        wrapflag, triginit;
    char direction, directionprev, directionorig, playfadeflag, recfadeflag, recendmark;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans;
    control_state_t   statecontrol;
    switchramp_type_t snrtype;
    interp_type_t     interp;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop, setloopsize = 0;
    long initiallow, initialhigh;

    t_buffer_obj* buf = buffer_ref_getobject(x->buffer.buf);
    float*        b = buffer_locksamples(buf);

    record = x->state.record;
    recordprev = x->state.recordprev;
    dirt = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt = 1;
    if (x->state.buf_modified) {
        karma_buf_modify(x, buf);
        x->state.buf_modified = false;
    }

    go = x->state.go;
    statecontrol = x->state.statecontrol;
    playfadeflag = x->fade.playfadeflag;
    recfadeflag = x->fade.recfadeflag;
    recordhead = x->timing.recordhead;
    alternateflag = x->state.alternateflag;
    pchans = x->buffer.bchans;
    // srscale = x->timing.srscale;
    frames = x->buffer.bframes;
    triginit = x->state.triginit;
    jumpflag = x->state.jumpflag;
    // append = x->state.append;
    directionorig = x->state.directionorig;
    direction = x->state.directionprev;
    directionprev = x->state.directionprev;
    // speed = 1.0;
    speedfloat = x->speedfloat;
    loopdetermine = x->state.loopdetermine;
    wrapflag = x->state.wrapflag;
    interp = x->audio.interpflag;
    accuratehead = x->timing.playhead;
    playhead = x->timing.playhead;
    // speedsrscaled = speed * srscale;
    // ovdbdif = 0.0;
    overdubamp = x->audio.overdubamp;
    overdubprev = x->audio.overdubprev;

    // Initialize arrays with individual struct members to avoid array bounds issues
    if (nchans > 0) { oprev[0] = x->audio.o1prev; odif[0] = x->audio.o1dif; }
    if (nchans > 1) { oprev[1] = x->audio.o2prev; odif[1] = x->audio.o2dif; }
    if (nchans > 2) { oprev[2] = x->audio.o3prev; odif[2] = x->audio.o3dif; }
    if (nchans > 3) { oprev[3] = x->audio.o4prev; odif[3] = x->audio.o4dif; }
    for (i = KARMA_STRUCT_CHANNEL_COUNT; i < nchans; i++) {
        oprev[i] = 0.0;
        odif[i] = 0.0;
    }

    // selstart = x->timing.selstart;
    // selection = x->timing.selection;
    startloop = x->loop.startloop;
    endloop = x->loop.endloop;
    minloop = x->loop.minloop;
    maxloop = x->loop.maxloop;
    setloopsize = maxloop - minloop;
    playfade = x->fade.playfade;
    recordfade = x->fade.recordfade;
    globalramp = x->fade.globalramp;
    snrramp = x->fade.snrramp;
    snrfade = x->fade.snrfade;
    snrtype = x->fade.snrtype;
    // initiallow = x->loop.initiallow;
    // initialhigh = x->loop.initialhigh;
    maxhead = x->timing.maxhead;
    // jumphead = x->timing.jumphead;
    // recplaydif = 0.0;
    pokesteps = x->audio.pokesteps;

    // Process state control using helper function
    kh_process_state_control(
        x, &statecontrol, &record, &go, &triginit, &loopdetermine, &recordfade,
        &recfadeflag, &playfade, &playfadeflag, &recendmark);

    while (n--) {
        for (i = 0; i < nchans; i++) {
            recin[i] = *ins[i]++;
        }
        speed = speedinlet ? *speed_in++ : speedfloat;
        direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

        kh_process_direction_change(x, b, directionprev, direction);
        if (directionprev != direction && record && globalramp) {
            recordhead = -1;
        }

        kh_process_record_toggle(x, b, accuratehead, direction, speed, &dirt);
        // recordprev = record;

        if (!loopdetermine) {
            if (go) {
                kh_process_loop_initialization(
                    x, b, &accuratehead, direction, &setloopsize, &wrapflag, &recendmark,
                    triginit, jumpflag);
                if (triginit) {
                    recordhead = -1;
                    triginit = 0;
                    if (record && !recendmark) {
                        recordfade = 0;
                        recfadeflag = 0;
                    }
                } else {
                    setloopsize = maxloop - minloop;
                    kh_process_loop_boundary(
                        x, b, &accuratehead, speed, direction, setloopsize, wrapflag,
                        jumpflag);

                    if (jumpflag) {
                        if (wrapflag) {
                            if ((accuratehead < endloop) || (accuratehead > startloop))
                                jumpflag = 0;
                        } else {
                            if ((accuratehead < endloop) && (accuratehead > startloop))
                                jumpflag = 0;
                        }
                    }
                }

                // Calculate multichannel interpolation and get all output samples
                kh_calculate_poly_interpolation_and_osamp(
                    accuratehead, direction, b, pchans, nchans, interp, directionorig, maxloop, frames, record, osamp);

                // Process multichannel ramps and fades
                kh_process_poly_ramps_and_fades(
                    osamp, oprev, odif, nchans, &snrfade, &playfade, globalramp, snrramp, snrtype,
                    &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine, record);
            } else {
                for (i = 0; i < nchans; i++) {
                    osamp[i] = 0.0;
                }
            }

            // Write each channel to its separate output buffer
            for (i = 0; i < nchans; i++) {
                *outs[multichannel_start_idx + i]++ = osamp[i];
                oprev[i] = osamp[i];
            }
            // Handle sync output if enabled
            if (syncoutlet) {
                setloopsize = maxloop - minloop;
                *sync_out++ = (directionorig >= 0) ?
                             ((accuratehead - minloop) / setloopsize) :
                             ((accuratehead - (frames - setloopsize)) / setloopsize);
            }

            if (record) {
                if ((recordfade < globalramp) && (globalramp > 0.0)) {
                    for (i = 0; i < nchans; i++) {
                        long chan_offset = i % pchans;
                        recin[i] = kh_ease_record(
                            recin[i] + (((double)b[playhead * pchans + chan_offset]) * overdubamp),
                            recfadeflag, globalramp, recordfade);
                    }
                    recordfade++;
                    if (recordfade >= globalramp)
                        kh_process_recording_fade_completion(
                            recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                            &loopdetermine, &recordfade, directionorig, &maxloop,
                            maxhead, globalramp);
                } else {
                    if (recfadeflag) {
                        kh_process_recording_fade_completion(
                            recfadeflag, &recendmark, &record, &triginit, &jumpflag,
                            &loopdetermine, &recordfade, directionorig, &maxloop, maxhead,
                            globalramp);
                    }
                }

                for (i = 0; i < nchans; i++) {
                    long chan_offset = i % pchans;
                    if (recordhead != -1) {
                        coeff1 = 1.0 / (pokesteps + 1.0);
                        if ((recordfade >= globalramp) || !recfadeflag) {
                            writeval1 = recin[i];
                        } else {
                            writeval1 = (((double)b[recordhead * pchans + chan_offset]) * (1.0 - coeff1)) + (recin[i] * coeff1);
                        }
                        b[recordhead * pchans + chan_offset] = writeval1;
                    }
                }
            }
        } else {
            playhead = trunc(accuratehead);

            if (globalramp) {
                if (playfade < globalramp) {
                    for (i = 0; i < nchans; i++) {
                        osamp[i] = kh_ease_record(0.0, (playfadeflag > 0), globalramp, playfade);
                    }
                    playfade++;
                    if (playfade >= globalramp) {
                        kh_process_playfade_state(
                            &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                            &playfade, &snrfade, record);
                    }
                } else {
                    for (i = 0; i < nchans; i++) {
                        osamp[i] = 0.0;
                    }
                }
            } else {
                for (i = 0; i < nchans; i++) {
                    osamp[i] = 0.0;
                }
            }

            // Write each channel to its separate output buffer
            for (i = 0; i < nchans; i++) {
                *outs[multichannel_start_idx + i]++ = osamp[i];
                oprev[i] = osamp[i];
            }
            // Handle sync output if enabled
            if (syncoutlet) {
                setloopsize = maxloop - minloop;
                *sync_out++ = (directionorig >= 0) ?
                             ((accuratehead - minloop) / setloopsize) :
                             ((accuratehead - (frames - setloopsize)) / setloopsize);
            }

            if (record) {
                for (i = 0; i < nchans; i++) {
                    long chan_offset = i % pchans;
                    if ((recordfade < globalramp) && (globalramp > 0.0)) {
                        recin[i] = kh_ease_record(
                            recin[i] + (((double)b[playhead * pchans + chan_offset]) * overdubamp),
                            recfadeflag, globalramp, recordfade);
                    } else {
                        recin[i] += ((double)b[playhead * pchans + chan_offset]) * overdubamp;
                    }

                    if (recordhead != -1) {
                        coeff1 = 1.0 / (pokesteps + 1.0);
                        writeval1 = (((double)b[recordhead * pchans + chan_offset]) * (1.0 - coeff1)) + (recin[i] * coeff1);
                        b[recordhead * pchans + chan_offset] = writeval1;
                    }
                }

                kh_process_recording_fade(
                    globalramp, &recordfade, &recfadeflag, &record, &triginit, &jumpflag);
            }
        }
        directionprev = direction;
    }

    // Update individual struct members to avoid array bounds issues
    if (nchans > 0) { x->audio.o1prev = oprev[0]; x->audio.o1dif = odif[0]; }
    if (nchans > 1) { x->audio.o2prev = oprev[1]; x->audio.o2dif = odif[1]; }
    if (nchans > 2) { x->audio.o3prev = oprev[2]; x->audio.o3dif = odif[2]; }
    if (nchans > 3) { x->audio.o4prev = oprev[3]; x->audio.o4dif = odif[3]; }

    x->state.record = record;
    x->state.recordprev = record;
    x->state.go = go;
    x->state.statecontrol = statecontrol;
    x->fade.playfadeflag = playfadeflag;
    x->fade.recfadeflag = recfadeflag;
    x->timing.recordhead = recordhead;
    x->state.alternateflag = alternateflag;
    x->state.directionprev = direction;
    x->speedfloat = speedfloat;
    x->state.loopdetermine = loopdetermine;
    x->state.wrapflag = wrapflag;
    x->timing.playhead = accuratehead;
    x->audio.overdubamp = overdubamp;
    x->audio.overdubprev = overdubprev;
    x->loop.startloop = startloop;
    x->loop.endloop = endloop;
    x->fade.playfade = playfade;
    x->fade.recordfade = recordfade;
    x->fade.snrfade = snrfade;
    x->state.triginit = triginit;
    x->state.jumpflag = jumpflag;

    if (dirt)
        buffer_setdirty(buf);
    buffer_unlocksamples(buf);

    return;

zero:
    while (n--) {
        // Write zeros to all multichannel output channels
        for (i = 0; i < nchans; i++) {
            *outs[multichannel_start_idx + i]++ = 0.0;
        }
        // Handle sync output if enabled
        if (syncoutlet)
            *sync_out++ = 0.0;
    }

    return;
}

// ============================== Helper Functions

// Helper functions for karma_buf_change_internal refactoring

static inline t_bool kh_validate_buffer(t_karma* x, t_symbol* bufname)
{
    return karma::validate_buffer(x, bufname);
}

static inline void kh_parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag)
{
    karma::parse_loop_points_sym(loop_points_sym, loop_points_flag);
}

static inline void kh_parse_numeric_arg(t_atom* arg, double* value)
{
    karma::parse_numeric_arg(arg, value);
}

static inline void kh_process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag)
{
    karma::process_argc_args(x, s, argc, argv, templow, temphigh, loop_points_flag);
}

// Helper functions for further karma_mono_perform refactoring

void kh_process_ipoke_recording(
    float* b, long pchans, long playhead, long* recordhead, double recin1,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, t_bool* dirt)
{
    karma::process_ipoke_recording(b, pchans, playhead, recordhead, recin1, overdubamp, globalramp,
                                    recordfade, recfadeflag, pokesteps, writeval1, dirt);
}

static inline void kh_process_recording_fade(
    double globalramp, long* recordfade, char* recfadeflag, t_bool* record,
    t_bool* triginit, t_bool* jumpflag)
{
    karma::process_recording_fade(globalramp, recordfade, recfadeflag, record, triginit, jumpflag);
}

static inline void kh_process_jump_logic(
    t_karma* x, float* b, double* accuratehead, t_bool* jumpflag, char direction)
{
    karma::process_jump_logic(x, b, accuratehead, jumpflag, direction);
}

static inline void kh_process_initial_loop_ipoke_recording(
    float* b, long pchans, long* recordhead, long playhead, double recin1,
    double* pokesteps, double* writeval1, char direction, char directionorig,
    long maxhead, long frames)
{
    karma::process_initial_loop_ipoke_recording(
        b, pchans, recordhead, playhead, recin1, pokesteps, writeval1,
        direction, directionorig, maxhead, frames);
}

static inline void kh_process_initial_loop_boundary_constraints(
    t_karma* x, float* b, double* accuratehead, double speed, char direction)
{
    karma::process_initial_loop_boundary_constraints(x, b, accuratehead, speed, direction);
}

// ============================== Stereo Helper Functions

void kh_process_ipoke_recording_stereo(
    float* b, long pchans, long playhead, long* recordhead, double recin1, double recin2,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, double* writeval2, t_bool* dirt)
{
    long   i;
    double recplaydif, coeff1, coeff2;

    // Handle first record head initialization
    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *writeval2 += recin2;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *writeval2 = *writeval2 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = *writeval1;
        if (pchans > 1) {
            b[*recordhead * pchans + 1] = *writeval2;
        }
        recplaydif = (double)(playhead - *recordhead);
        if (recplaydif > 0) { // linear-interpolation for speed > 1x
            coeff1 = (recin1 - *writeval1) / recplaydif;
            coeff2 = (recin2 - *writeval2) / recplaydif;
            for (i = *recordhead + 1; i < playhead; i++) {
                *writeval1 += coeff1;
                *writeval2 += coeff2;
                b[i * pchans] = *writeval1;
                if (pchans > 1) {
                    b[i * pchans + 1] = *writeval2;
                }
            }
        } else {
            coeff1 = (recin1 - *writeval1) / recplaydif;
            coeff2 = (recin2 - *writeval2) / recplaydif;
            for (i = *recordhead - 1; i > playhead; i--) {
                *writeval1 -= coeff1;
                *writeval2 -= coeff2;
                b[i * pchans] = *writeval1;
                if (pchans > 1) {
                    b[i * pchans + 1] = *writeval2;
                }
            }
        }
        *writeval1 = recin1;
        *writeval2 = recin2;
    }
    *recordhead = playhead;
    *dirt = 1;
}

static inline void kh_process_initial_loop_ipoke_recording_stereo(
    float* b, long pchans, long* recordhead, long playhead, double recin1, double recin2,
    double* pokesteps, double* writeval1, double* writeval2,
    char direction, char directionorig, long maxhead, long frames)
{
    long   i;
    double recplaydif, coeff1, coeff2;

    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *writeval2 += recin2;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *writeval2 = *writeval2 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = *writeval1;
        if (pchans > 1) {
            b[*recordhead * pchans + 1] = *writeval2;
        }
        recplaydif = (double)(playhead - *recordhead); // linear-interp for speed > 1x

        if (direction != directionorig) {
            if (directionorig >= 0) {
                if (recplaydif > 0) {
                    if (recplaydif > (maxhead * 0.5)) {
                        recplaydif -= maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i >= 0; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                        for (i = maxhead; i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    }
                } else {
                    if ((-recplaydif) > (maxhead * 0.5)) {
                        recplaydif += maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < (maxhead + 1); i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                        for (i = 0; i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    }
                }
            } else {
                if (recplaydif > 0) {
                    if (recplaydif > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif -= ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i >= maxhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                        for (i = (frames - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    }
                } else {
                    if ((-recplaydif) > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif += ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < frames; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                        for (i = maxhead; i > playhead; i--) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = *writeval1;
                            if (pchans > 1) {
                                b[i * pchans + 1] = *writeval2;
                            }
                        }
                    }
                }
            }
        } else {
            if (recplaydif > 0) { // linear-interpolation for speed > 1x
                coeff1 = (recin1 - *writeval1) / recplaydif;
                coeff2 = (recin2 - *writeval2) / recplaydif;
                for (i = *recordhead + 1; i < playhead; i++) {
                    *writeval1 += coeff1;
                    *writeval2 += coeff2;
                    b[i * pchans] = *writeval1;
                    if (pchans > 1) {
                        b[i * pchans + 1] = *writeval2;
                    }
                }
            } else {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                coeff2 = (recin2 - *writeval2) / recplaydif;
                for (i = *recordhead - 1; i > playhead; i--) {
                    *writeval1 -= coeff1;
                    *writeval2 -= coeff2;
                    b[i * pchans] = *writeval1;
                    if (pchans > 1) {
                        b[i * pchans + 1] = *writeval2;
                    }
                }
            }
        }
        *writeval1 = recin1;
        *writeval2 = recin2;
    }
}
