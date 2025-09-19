#include "karma.h"

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
        //  double  selmultiply;    // store loop length multiplier amount from 'multiply' method -->> TODO
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
        char    playfadeflag;   // playback up/down flag, used as: 0 = fade up/in, 1 = fade down/out (<<-- TODO: reverse ??) but case switch 0..4 ??
        char    recfadeflag;    // record up/down flag, 0 = fade up/in, 1 = fade down/out (<<-- TODO: reverse ??) but used 0..5 ??
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

    double  speedfloat;     // store speed inlet value if float (not signal)

    long    syncoutlet;     // make sync outlet ? (object attribute @syncout, instantiation time only)
//  long    boffset;        // zero indexed buffer channel # (default 0), user settable, not buffer~ queried -->> TODO
    long    moduloout;      // modulo playback channel outputs flag, user settable, not buffer~ queried -->> TODO
    long    islooped;       // can disable/enable global looping status (rodrigo @ttribute request, TODO) (!! long ??)

    long   recordhead;      // record head position in samples
    long   reportlist;      // right list outlet report granularity in ms (!! why is this a long ??)

    short   speedconnect;   // 'count[]' info for 'speed' as signal or float in perform routines

    // Multichannel processing arrays (pre-allocated to avoid real-time allocation)
    double  *poly_osamp;    // output sample arrays for multichannel
    double  *poly_oprev;    // previous output arrays for multichannel
    double  *poly_odif;     // output difference arrays for multichannel
    double  *poly_recin;    // record input arrays for multichannel
    long    poly_maxchans;  // maximum allocated channel count
    long    input_channels; // current input channel count for auto-adapting

    void    *messout;       // list outlet pointer
    void    *tclock;        // list timer pointer
};


static t_symbol *ps_nothing;
static t_symbol *ps_dummy; 
static t_symbol *ps_buffer_modified;
static t_symbol *ps_phase;
static t_symbol *ps_samples;
static t_symbol *ps_milliseconds;
static t_symbol *ps_originalloop;

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

static inline void kh_process_state_control(
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

static inline double kh_process_audio_interpolation(
    float* b, long pchans, double accuratehead, interp_type_t interp, t_bool record);
void kh_process_ipoke_recording_stereo(
    float* b, long pchans, long playhead, long* recordhead, double recin1, double recin2,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, double* writeval2, t_bool* dirt);
static inline void kh_process_initial_loop_ipoke_recording_stereo(
    float* b, long pchans, long* recordhead, long playhead, double recin1, double recin2,
    double* pokesteps, double* writeval1, double* writeval2,
    char direction, char directionorig, long maxhead, long frames);


// --------------------------------------------------------------------------------------


// Linear Interp
static inline double kh_linear_interp(double f, double x, double y)
{
    return (x + f*(y - x));
}

// Hermitic Cubic Interp, 4-point 3rd-order, ( James McCartney / Alex Harker )
static inline double kh_cubic_interp(double f, double w, double x, double y, double z)
{
    return ((((0.5*(z - w) + 1.5*(x - y))*f + (w - 2.5*x + y + y - 0.5*z))*f + (0.5*(y - w)))*f + x);
}

// Catmull-Rom Spline Interp, 4-point 3rd-order, ( Paul Breeuwsma / Paul Bourke )
static inline double kh_spline_interp(double f, double w, double x, double y, double z)
{
    return (((-0.5*w + 1.5*x - 1.5*y + 0.5*z)*pow(f,3)) + ((w - 2.5*x + y + y - 0.5*z)*pow(f,2)) + ((-0.5*w + 0.5*y)*f) + x);
}


// easing function for recording (with ipoke)
static inline double kh_ease_record(double y1, char updwn, double globalramp, long playfade)
{
    double ifup    = (1.0 - (((double)playfade) / globalramp)) * PI;
    double ifdown  = (((double)playfade) / globalramp) * PI;
    return updwn ? y1 * (0.5 * (1.0 - cos(ifup))) : y1 * (0.5 * (1.0 - cos(ifdown)));
}

// easing function for switch & ramp
static inline double kh_ease_switchramp(double y1, double snrfade, switchramp_type_t snrtype)
{
    switch (snrtype)
    {
        case SWITCHRAMP_LINEAR: 
            y1  = y1 * (1.0 - snrfade);
            break;
        case SWITCHRAMP_SINE_IN: 
            y1  = y1 * (1.0 - (sin((snrfade - 1) * PI/2) + 1));
            break;
        case SWITCHRAMP_CUBIC_IN: 
            y1  = y1 * (1.0 - (snrfade * snrfade * snrfade));
            break;
        case SWITCHRAMP_CUBIC_OUT: 
            snrfade = snrfade - 1;
            y1  = y1 * (1.0 - (snrfade * snrfade * snrfade + 1));
            break;
        case SWITCHRAMP_EXPO_IN: 
            snrfade = (snrfade == 0.0) ? snrfade : pow(2, (10 * (snrfade - 1)));
            y1  = y1 * (1.0 - snrfade);
            break;
        case SWITCHRAMP_EXPO_OUT: 
            snrfade = (snrfade == 1.0) ? snrfade : (1 - pow(2, (-10 * snrfade)));
            y1  = y1 * (1.0 - snrfade);
            break;
        case SWITCHRAMP_EXPO_IN_OUT: 
            if ((snrfade > 0) && (snrfade < 0.5))
                y1 = y1 * (1.0 - (0.5 * pow(2, ((20 * snrfade) - 10))));
            else if ((snrfade < 1) && (snrfade > 0.5))
                y1 = y1 * (1.0 - (-0.5 * pow(2, ((-20 * snrfade) + 10)) + 1));
            break;
    }
    return  y1;
}

// easing function for buffer read
static inline void kh_ease_bufoff(long framesm1, float *buf, long pchans, long markposition, char direction, double globalramp)
{
    long i, fadpos, c;
    double fade;

    if (globalramp <= 0) return;

    for (i = 0; i < globalramp; i++)
    {
        fadpos = markposition + (direction * i);

        if (fadpos < 0 || fadpos > framesm1)
            continue;

        fade = 0.5 * (1.0 - cos(((double)i / globalramp) * PI));

        for (c = 0; c < pchans; c++)
        {
            buf[(fadpos * pchans) + c] *= fade;
        }
    }
}

static inline void kh_apply_fade(long pos, long framesm1, float *buf, long pchans, double fade)
{
    if (pos < 0 || pos > framesm1)
        return;
    for (long c = 0; c < pchans; c++) {
        buf[(pos * pchans) + c] *= fade;
    }
};


// easing function for buffer write
static inline void kh_ease_bufon(
    long framesm1, float *buf, long pchans, long markposition1, long markposition2, 
    char direction, double globalramp)
{
    long fadpos[3];
    double fade;

    for (long i = 0; i < globalramp; i++)
    {
        fade = 0.5 * (1.0 - cos(((double)i / globalramp) * PI));
        fadpos[0] = (markposition1 - direction) - (direction * i);
        fadpos[1] = (markposition2 - direction) - (direction * i);
        fadpos[2] =  markposition2 + (direction * i);

                    kh_apply_fade(fadpos[0], framesm1, buf, pchans, fade);
            kh_apply_fade(fadpos[1], framesm1, buf, pchans, fade);
            kh_apply_fade(fadpos[2], framesm1, buf, pchans, fade);
    }
}

// Helper function to handle recording fade completion logic
static inline void kh_process_recording_fade_completion(
    char recfadeflag, char *recendmark, t_bool *record, 
    t_bool *triginit, t_bool *jumpflag, t_bool *loopdetermine,
    long *recordfade, char directionorig, long *maxloop, 
    long maxhead, long frames)
{

    if (recfadeflag == 2) {
        *recendmark = 4;
        *triginit = *jumpflag = 1;
        *recordfade = 0;
    } else if (recfadeflag == 5) {
        *record = 1;
    }
    
    switch (*recendmark) {
        case 0:
            *record = 0;
            break;
        case 1:
            if (directionorig < 0) {
                *maxloop = (frames - 1) - maxhead;
            } else {
                *maxloop = maxhead;
            }
        case 2:
            *record = *loopdetermine = 0;
            *triginit = 1;
            break;
        case 3:
            *record = *triginit = 1;
            *recordfade = *loopdetermine = 0;
            break;
        case 4:
            *recendmark = 0;
            break;
    }
}

// Helper function to calculate sync outlet output
static inline void kh_calculate_sync_output(
    double osamp1, double *o1prev, double **out1, char syncoutlet,
    double **outPh, double accuratehead, double minloop, double maxloop,
    char directionorig, long frames, double setloopsize)
{
    *o1prev = osamp1;
    *(*out1)++ = osamp1;
    if (syncoutlet) {
        setloopsize = maxloop - minloop;
        *(*outPh)++ = (directionorig >= 0) ? 
                      ((accuratehead - minloop) / setloopsize) : 
                      ((accuratehead - (frames - setloopsize)) / setloopsize);
    }
}

// Helper function to apply iPoke interpolation over a range
static inline void kh_apply_ipoke_interpolation(
    float *b, long pchans, long start_idx, long end_idx,
    double *writeval1, double coeff1, char direction) 
{
    if (direction > 0) {
        for (long i = start_idx; i < end_idx; i++) {
            *writeval1 += coeff1;
            b[i * pchans] = *writeval1;
        }
    } else {
        for (long i = start_idx; i > end_idx; i--) {
            *writeval1 -= coeff1;
            b[i * pchans] = *writeval1;
        }
    }
}

// Helper function to initialize buffer properties
static inline void kh_init_buffer_properties(t_karma *x, t_buffer_obj *buf) {
    x->buffer.bchans   = buffer_getchannelcount(buf);
    x->buffer.bframes  = buffer_getframecount(buf);
    x->buffer.bmsr     = buffer_getmillisamplerate(buf);
    x->buffer.bsr      = buffer_getsamplerate(buf);
    x->buffer.nchans   = (x->buffer.bchans < x->buffer.ochans) ? x->buffer.bchans : x->buffer.ochans;  // MIN
    x->timing.srscale  = x->buffer.bsr / x->timing.ssr;
}

// Helper function to handle loop boundary wrapping and jumping
// Helper function to handle recording state cleanup after boundary adjustments
static inline void kh_process_recording_cleanup(
    t_karma *x, float *b, double accuratehead, char direction, t_bool use_ease_on, double ease_pos)
{
    x->fade.snrfade = 0.0;
    if (x->state.record) {
        if (x->fade.globalramp) {
            if (use_ease_on) {
                kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, 
                             accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
            } else {
                kh_ease_bufoff(x->buffer.bframes - 1, b, x->buffer.nchans, 
                              ease_pos, -direction, x->fade.globalramp);
            }
            x->fade.recordfade = 0;
        }
        x->fade.recfadeflag = 0;
        x->timing.recordhead = -1;
    }
}

// Helper function to handle forward direction boundary wrapping for jumpflag
static inline void kh_process_forward_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    if (*accuratehead > x->loop.maxloop) {
        *accuratehead = *accuratehead - (x->loop.maxloop - x->loop.minloop);
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    } else if (*accuratehead < 0.0) {
        *accuratehead = x->loop.maxloop + *accuratehead;
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    }
}

// Helper function to handle reverse direction boundary wrapping for jumpflag
static inline void kh_process_reverse_jump_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    long setloopsize = x->loop.maxloop - x->loop.minloop;
    if (*accuratehead > (x->buffer.bframes - 1)) {
        *accuratehead = ((x->buffer.bframes - 1) - setloopsize) + (*accuratehead - (x->buffer.bframes - 1));
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    } else if (*accuratehead < ((x->buffer.bframes - 1) - x->loop.maxloop)) {
        *accuratehead = (x->buffer.bframes - 1) - (((x->buffer.bframes - 1) - setloopsize) - *accuratehead);
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    }
}

// Helper function to handle forward direction boundaries with wrapflag
static inline void kh_process_forward_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    long setloopsize = x->loop.maxloop - x->loop.minloop;
    if (*accuratehead > x->loop.maxloop) {
        *accuratehead = *accuratehead - setloopsize;
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 0, x->loop.maxloop);
    } else if (*accuratehead < 0.0) {
        *accuratehead = x->loop.maxloop + setloopsize;
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 0, x->loop.minloop);
    }
}

// Helper function to handle reverse direction boundaries with wrapflag
static inline void kh_process_reverse_wrap_boundary(
    t_karma *x, float *b, double *accuratehead, char direction)
{
    long setloopsize = x->loop.maxloop - x->loop.minloop;
    if (*accuratehead < ((x->buffer.bframes - 1) - x->loop.maxloop)) {
        *accuratehead = (x->buffer.bframes - 1) - (((x->buffer.bframes - 1) - setloopsize) - *accuratehead);
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 0, ((x->buffer.bframes - 1) - x->loop.maxloop));
    } else if (*accuratehead > (x->buffer.bframes - 1)) {
        *accuratehead = ((x->buffer.bframes - 1) - setloopsize) + (*accuratehead - (x->buffer.bframes - 1));
        kh_process_recording_cleanup(x, b, *accuratehead, direction, 0, (x->buffer.bframes - 1));
    }
}

static inline void kh_process_loop_boundary(
    t_karma *x, float *b, double *accuratehead, double speed, char direction, 
    long setloopsize, t_bool wrapflag, t_bool jumpflag)
{
    double speedsrscaled = speed * x->timing.srscale;
    
    if (x->state.record) {
        speedsrscaled = (fabs(speedsrscaled) > (setloopsize / 1024)) ? 
                       ((setloopsize / 1024) * direction) : speedsrscaled;
    }
    *accuratehead = *accuratehead + speedsrscaled;
    
    if (jumpflag) {
        // Handle boundary wrapping for forward/reverse directions
        if (x->state.directionorig >= 0) {
            kh_process_forward_jump_boundary(x, b, accuratehead, direction);
        } else {
            kh_process_reverse_jump_boundary(x, b, accuratehead, direction);
        }
    } else {
        // Regular window/position constraints handling
        if (wrapflag) {
            if ((*accuratehead > x->loop.endloop) && (*accuratehead < x->loop.startloop)) {
                *accuratehead = (direction >= 0) ? x->loop.startloop : x->loop.endloop;
                kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
            } else if (x->state.directionorig >= 0) {
                kh_process_forward_wrap_boundary(x, b, accuratehead, direction);
            } else {
                kh_process_reverse_wrap_boundary(x, b, accuratehead, direction);
            }
        } else {
            // Not wrapflag
            if ((*accuratehead > x->loop.endloop) || (*accuratehead < x->loop.startloop)) {
                *accuratehead = (direction >= 0) ? x->loop.startloop : x->loop.endloop;
                kh_process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
            }
        }
    }
}

// Helper function to perform playback interpolation
static inline double kh_perform_playback_interpolation(
    double frac, float *b, long interp0, long interp1, 
    long interp2, long interp3, long pchans, 
    interp_type_t interp, t_bool record)
{
    if (record) {
        // If recording, use linear interpolation
        return kh_linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
    } else {
        // Otherwise use specified interpolation type
        if (interp == 1) {
            return kh_cubic_interp(frac, b[interp0 * pchans], b[interp1 * pchans], b[interp2 * pchans], b[interp3 * pchans]);
        } else if (interp == 2) {
            return kh_spline_interp(frac, b[interp0 * pchans], b[interp1 * pchans], b[interp2 * pchans], b[interp3 * pchans]);
        } else {
            return kh_linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
        }
    }
}

// Helper function to handle playfade state machine logic
static inline void kh_process_playfade_state(
    char *playfadeflag, t_bool *go, t_bool *triginit, t_bool *jumpflag, 
    t_bool *loopdetermine, long *playfade, double *snrfade, t_bool record)
{
    switch (*playfadeflag) {
        case 0:
            break;
        case 1:
            *playfadeflag = *go = 0;
            break;
        case 2:
            if (!record)
                *triginit = *jumpflag = 1;
            // Fall through to case 3
        case 3:
            *playfadeflag = *playfade = 0;
            break;
        case 4:  // append
            *go = *triginit = *loopdetermine = 1;
            *snrfade = 0.0;
            *playfade = 0;
            *playfadeflag = 0;
            break;
    }
}

// Helper function to handle loop initialization and calculation
static inline void kh_process_loop_initialization(
    t_karma *x, float *b, double *accuratehead, char direction,
    long *setloopsize, t_bool *wrapflag, char *recendmark_ptr,
    t_bool triginit, t_bool jumpflag)
{
    if (triginit) {
        if (x->state.recendmark) {  // calculate end of loop
            if (x->state.directionorig >= 0) {
                x->loop.maxloop = CLAMP(x->timing.maxhead, 4096, x->buffer.bframes - 1);
                *setloopsize = x->loop.maxloop - x->loop.minloop;
                *accuratehead = x->loop.startloop = x->loop.minloop + (x->timing.selstart * (*setloopsize));
                x->loop.endloop = x->loop.startloop + (x->timing.selection * (*setloopsize));
                if (x->loop.endloop > x->loop.maxloop) {
                    x->loop.endloop = x->loop.endloop - ((*setloopsize) + 1);
                    *wrapflag = 1;
                } else {
                    *wrapflag = 0;
                }
                if (direction < 0) {
                    if (x->fade.globalramp)
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                }
            } else {
                x->loop.maxloop = CLAMP((x->buffer.bframes - 1) - x->timing.maxhead, 4096, x->buffer.bframes - 1);
                *setloopsize = x->loop.maxloop - x->loop.minloop;
                x->loop.startloop = ((x->buffer.bframes - 1) - (*setloopsize)) + (x->timing.selstart * (*setloopsize));
                if (x->loop.endloop > (x->buffer.bframes - 1)) {
                    x->loop.endloop = ((x->buffer.bframes - 1) - (*setloopsize)) + (x->loop.endloop - x->buffer.bframes);
                    *wrapflag = 1;
                } else {
                    *wrapflag = 0;
                }
                *accuratehead = x->loop.endloop;
                if (direction > 0) {
                    if (x->fade.globalramp)
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                }
            }
            if (x->fade.globalramp)
                kh_ease_bufoff(x->buffer.bframes - 1, b, x->buffer.nchans, x->timing.maxhead, -direction, x->fade.globalramp);
            x->fade.snrfade = 0.0;
            x->state.append = x->state.alternateflag = 0;
            *recendmark_ptr = 0;
        } else {    // jump / play (inside 'window')
            *setloopsize = x->loop.maxloop - x->loop.minloop;
            if (jumpflag)
                *accuratehead = (x->state.directionorig >= 0) ? ((x->timing.jumphead * (*setloopsize)) + x->loop.minloop) : (((x->buffer.bframes - 1) - (x->loop.maxloop)) + (x->timing.jumphead * (*setloopsize)));
            else
                *accuratehead = (direction < 0) ? x->loop.endloop : x->loop.startloop;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                }
            }
            x->fade.snrfade = 0.0;
        }
    }
}

// Helper function to handle initial loop creation state
static inline void kh_process_initial_loop_creation(
    t_karma *x, float *b, double *accuratehead, char direction, t_bool *triginit_ptr)
{
    if (x->state.go) {
        if (x->state.triginit) {
            if (x->state.jumpflag) {
                // Jump logic handled by existing karma_handle_jump_logic function
            } else if (x->state.append) {
                x->fade.snrfade = 0.0;
                *triginit_ptr = 0;
                if (x->state.record) {
                    *accuratehead = x->timing.maxhead;
                    if (x->fade.globalramp) {
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                        x->fade.recordfade = 0;
                    }
                    x->state.alternateflag = 1;
                    x->fade.recfadeflag = 0;
                    x->timing.recordhead = -1;
                } else {
                    *accuratehead = (x->state.directionorig >= 0) ? 0.0 : (x->buffer.bframes - 1);
                    if (x->fade.globalramp) {
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                    }
                }
            } else {  // regular start
                x->fade.snrfade = 0.0;
                *triginit_ptr = 0;
                *accuratehead = (x->state.directionorig >= 0) ? 0.0 : (x->buffer.bframes - 1);
                if (x->state.record) {
                    if (x->fade.globalramp) {
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                        x->fade.recordfade = 0;
                    }
                    x->fade.recfadeflag = 0;
                    x->timing.recordhead = -1;
                } else {
                    if (x->fade.globalramp) {
                        kh_ease_bufon(x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead, x->timing.recordhead, direction, x->fade.globalramp);
                    }
                }
            }
        }
    }
}

// interpolation points
// Helper to wrap index for forward or reverse looping
static inline long kh_wrap_index(long idx, char directionorig, long maxloop, long framesm1) {
    if (directionorig >= 0) {
        // Forward: wrap between 0 and maxloop
        if (idx < 0)
            return (maxloop + 1) + idx;
        else if (idx > maxloop)
            return idx - (maxloop + 1);
        else
            return idx;
    } else {
        // Reverse: wrap between (framesm1 - maxloop) and framesm1
        long min = framesm1 - maxloop;
        if (idx < min)
            return framesm1 - (min - idx);
        else if (idx > framesm1)
            return min + (idx - framesm1);
        else
            return idx;
    }
}

static inline void kh_interp_index(
    long playhead, long *indx0, long *indx1, long *indx2, long *indx3,
    char direction, char directionorig, long maxloop, long framesm1)
{
    *indx0 = kh_wrap_index(playhead - direction, directionorig, maxloop, framesm1);
    *indx1 = playhead;
    *indx2 = kh_wrap_index(playhead + direction, directionorig, maxloop, framesm1);
    *indx3 = kh_wrap_index(*indx2 + direction, directionorig, maxloop, framesm1);
}


void ext_main(void *r)
{
    t_class *c = class_new("karma~", (method)karma_new, (method)karma_free, (long)sizeof(t_karma), 0L, A_GIMME, 0);

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
    CLASS_ATTR_FILTER_CLIP(c, "ramp", 0, 2048);
    CLASS_ATTR_LABEL(c, "ramp", 0, "Ramp Time (samples)");
    
    CLASS_ATTR_LONG(c, "snramp", 0, t_karma, fade.snrramp);          // !! change this to ms input !!    // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "snramp", 0, 2048);
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
        x->poly_maxchans = (chans > 4) ? chans : 4;  // Minimum 4, supports up to chans
        x->poly_osamp = (double*)sysmem_newptrclear(x->poly_maxchans * sizeof(double));
        x->poly_oprev = (double*)sysmem_newptrclear(x->poly_maxchans * sizeof(double));
        x->poly_odif = (double*)sysmem_newptrclear(x->poly_maxchans * sizeof(double));
        x->poly_recin = (double*)sysmem_newptrclear(x->poly_maxchans * sizeof(double));
        x->input_channels = chans;  // Initialize input channel count

        x->timing.recordhead = -1;
        x->reportlist = 50;                          // ms
        x->fade.snrramp = x->fade.globalramp = 256;  // samps...
        x->fade.playfade = x->fade.recordfade = 257; // ...
        x->timing.ssr = sys_getsr();
        x->timing.vs = sys_getblksize();
        x->timing.vsnorm = x->timing.vs / x->timing.ssr;

        x->audio.overdubprev = 1.0;
        x->audio.overdubamp = 1.0;
        x->speedfloat = 1.0;
        x->islooped = 1;

        x->fade.snrtype = SWITCHRAMP_SINE_IN;
        x->audio.interpflag = INTERP_CUBIC;
        x->fade.playfadeflag = 0;
        x->fade.recfadeflag = 0;
        x->state.recordinit = 0;
        x->state.initinit = 0;
        x->state.append = 0;
        x->state.jumpflag = 0;
        x->state.statecontrol = CONTROL_STATE_ZERO;
        x->state.statehuman = HUMAN_STATE_STOP;
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
        x->tclock = clock_new((t_object*)x, (method)karma_clock_list);
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

        // Free multichannel processing arrays
        if (x->poly_osamp) sysmem_freeptr(x->poly_osamp);
        if (x->poly_oprev) sysmem_freeptr(x->poly_oprev);
        if (x->poly_odif) sysmem_freeptr(x->poly_odif);
        if (x->poly_recin) sysmem_freeptr(x->poly_recin);

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

// called by 'karma_buf_change_internal' & 'karma_setloop_internal'
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
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
    // regardless of input choice ('loop_points_flag'), final low/high system is
    // normalised (& clipped) 0..1 (phase)
    low = CLAMP(low, 0., 1.);
    high = CLAMP(high, 0., 1.);
    /*
        // dev
        loop_points_sym = (loop_points_flag > 1) ? ps_milliseconds :
       ((loop_points_flag < 1) ? ps_phase : ps_samples); post("loop start
       normalised %.2f, loop end normalised %.2f, units %s", low, high,
       *loop_points_sym);
        //post("loop start samples %.2f, loop end samples %.2f, units used %s",
       (low * bframesm1), (high * bframesm1), *loop_points_sym);
    */
    // to samples, and account for channels & buffer samplerate
    /*    if (bchans > 1) {
            low     = (low * bframesm1) * bchans;// + channeloffset;
            high    = (high * bframesm1) * bchans;// + channeloffset;
        } else {
            low     = (low * bframesm1);// + channeloffset;
            high    = (high * bframesm1);// + channeloffset;
        }
        x->minloop = x->startloop = low;
        x->maxloop = x->endloop = high;
    */
    x->loop.minloop = x->loop.startloop = low * bframesm1;
    x->loop.maxloop = x->loop.endloop = high * bframesm1;

    // update selection
    karma_select_size(x, x->timing.selection);
    karma_select_start(x, x->timing.selstart);
    // karma_select_internal(x, x->timing.selstart, x->timing.selection);
}

// karma_buf_change method defered
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
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
    if (templow == -999.0) {
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

// karma_setloop method (defered?)
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
void kh_setloop_internal(
    t_karma* x, t_symbol* s, short argc, t_atom* argv) // " setloop ..... "
{
    t_bool    callerid = false; // identify caller of 'karma_buf_values_internal()'
    t_symbol* loop_points_sym = 0;
    long      loop_points_flag; // specify start/end loop points: 0 = in phase, 1 =
                                // in samples, 2 = in milliseconds (default)
    double templow, temphigh, temphightemp;

    // !! if just "setloop" with no additional args...
    // ...message will reset loop points to min / max !!
    loop_points_flag = 2;
    templow = -1.;
    temphigh = -1.;

    // maximum length message (3 atoms after 'setloop') = " setloop ...
    // ... 0::float::loop start/size [1::float::loop end] [2::symbol::loop
    // points type] "

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
        atom_setlong(datalist + 6, statehuman);              // state flag int

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
                    strncpy_zero(s, "(signal) Record Input / messages to karma~", 256);
                else
                    strncpy_zero(s, "(signal) Record Input 1 / messages to karma~", 256);
            } else {
                snprintf_zero(s, 256, "(signal) Record Input %ld", dummy);
                // @in 0 @type signal @digest Audio Inlet(s)... (object arg #2)
            }
            break;
        case 1:
            strncpy_zero(s, "(signal/float) Speed Factor (1. == normal speed)", 256);
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
                strncpy_zero(s, "(signal) Audio Output", 256);
            else
                snprintf_zero(s, 256, "(signal) Audio Output %ld", dummy);
            // @out 0 @type signal @digest Audio Outlet(s)... (object arg #2)
            break;
        case 1:
            if (synclet)
                strncpy_zero(s, "(signal) Sync Outlet (current position 0..1)", 256);
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
            x->state.statecontrol = x->state.alternateflag ? CONTROL_STATE_STOP_ALT
                                                           : CONTROL_STATE_STOP_REGULAR;
            x->state.append = 0;
            x->state.statehuman = HUMAN_STATE_STOP;
            x->state.stopallowed = 0;
        }
    }
}

void karma_play(t_karma* x)
{
    if ((!x->state.go) && (x->state.append)) {
        x->state.statecontrol = CONTROL_STATE_APPEND;

        x->fade.snrfade = 0.0; // !! should disable ??
    } else if ((x->state.record) || (x->state.append)) {
        x->state.statecontrol = x->state.alternateflag ? CONTROL_STATE_PLAY_ALT
                                                       : CONTROL_STATE_RECORD_OFF;
    } else {
        x->state.statecontrol = CONTROL_STATE_PLAY_ON;
    }

    x->state.go = 1;
    x->state.statehuman = HUMAN_STATE_PLAY;
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
    control_state_t sc = CONTROL_STATE_ZERO;
    human_state_t   sh = x->state.statehuman;
    t_bool          record = x->state.record;
    t_bool          go = x->state.go;
    t_bool          altflag = x->state.alternateflag;
    t_bool          append = x->state.append;
    t_bool          init = x->state.recordinit;

    x->state.stopallowed = 1;

    if (record) {
        if (altflag) {
            sc = CONTROL_STATE_RECORD_ALT;
            sh = HUMAN_STATE_OVERDUB;
        } else {
            sc = CONTROL_STATE_RECORD_OFF;
            sh = (sh == HUMAN_STATE_OVERDUB) ? HUMAN_STATE_PLAY : HUMAN_STATE_RECORD;
        }
    } else if (append) {
        if (go) {
            if (altflag) {
                sc = CONTROL_STATE_RECORD_ALT;
                sh = HUMAN_STATE_OVERDUB;
            } else {
                sc = CONTROL_STATE_APPEND_SPECIAL;
                sh = HUMAN_STATE_APPEND;
            }
        } else {
            sc = CONTROL_STATE_RECORD_INITIAL_LOOP;
            sh = HUMAN_STATE_INITIAL;
        }
    } else if (!go) {
        init = 1;
        if (buf) {
            long rchans = x->buffer.bchans;
            long bframes = x->buffer.bframes;
            _clear_buffer(buf, bframes, rchans);
        }
        sc = CONTROL_STATE_RECORD_INITIAL_LOOP;
        sh = HUMAN_STATE_INITIAL;
    } else {
        sc = CONTROL_STATE_RECORD_ON;
        sh = HUMAN_STATE_OVERDUB;
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
            x->state.statecontrol = CONTROL_STATE_APPEND;
            x->state.statehuman = HUMAN_STATE_APPEND;
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
            x->state.statecontrol = CONTROL_STATE_JUMP;
            x->timing.jumphead = CLAMP(
                jumpposition, 0.,
                1.); // for now phase only, TODO - ms & samples
            //          x->state.statehuman = HUMAN_STATE_PLAY;           // no -
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
    t_bool* alternateflag = &x->state.alternateflag;
    double* snrfade = &x->fade.snrfade;

    switch (*statecontrol) // "all-in-one 'switch' statement to catch and handle
                           // all(most) messages" - raja
    {
    case CONTROL_STATE_ZERO:
        break;
    case CONTROL_STATE_RECORD_INITIAL_LOOP:
        *record = *go = *triginit = *loopdetermine = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *recfadeflag = *playfade = *playfadeflag = 0;
        break;
    case CONTROL_STATE_RECORD_ALT:
        *recendmark = 3;
        *record = *recfadeflag = *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_RECORD_OFF:
        *recfadeflag = 1;
        *playfadeflag = 3;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_PLAY_ALT:
        *recendmark = 2;
        *recfadeflag = *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_PLAY_ON:
        *triginit = 1; // ?!?!
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_STOP_ALT:
        *playfade = *recordfade = 0;
        *recendmark = *playfadeflag = *recfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_STOP_REGULAR:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 1;
        }
        *playfade = 0;
        *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_JUMP:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 2;
        }
        *playfade = 0;
        *playfadeflag = 2;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_APPEND:
        *playfadeflag = 4; // !! modified in perform loop switch case(s) for
                           // playing behind append
        *playfade = 0;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_APPEND_SPECIAL:
        *record = *loopdetermine = *alternateflag = 1;
        *snrfade = 0.0;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *recfadeflag = 0;
        break;
    case CONTROL_STATE_RECORD_ON:
        *playfadeflag = 3;
        *recfadeflag = 5;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *playfade = 0;
        break;
    }
}

// Helper function: Initialize performance variables
static inline void kh_initialize_perform_vars(
    t_karma* x, double* accuratehead, long* playhead, t_bool* wrapflag)
{
    // Most variables now accessed directly from struct, only essential ones passed out
    *accuratehead = x->timing.playhead;
    *playhead = trunc(*accuratehead);
    *wrapflag = x->state.wrapflag;
}

// Helper function: Handle direction changes
static inline void kh_process_direction_change(t_karma* x, float* b, char directionprev, char direction)
{
    if (directionprev != direction) {
        if (x->state.record && x->fade.globalramp) {
            kh_ease_bufoff(
                x->buffer.bframes - 1, b, x->buffer.nchans, x->timing.recordhead,
                -direction, x->fade.globalramp);
            x->fade.recordfade = x->fade.recfadeflag = 0;
            // recordhead = -1; // Note: this should be handled by caller
        }
        x->fade.snrfade = 0.0;
    }
}

// Helper function: Handle record on/off transitions
static inline void kh_process_record_toggle(
    t_karma* x, float* b, double accuratehead, char direction, double speed, t_bool* dirt)
{
    if ((x->state.record - x->state.recordprev) < 0) { // samp @record-off
        if (x->fade.globalramp)
            kh_ease_bufoff(
                x->buffer.bframes - 1, b, x->buffer.nchans, x->timing.recordhead,
                direction, x->fade.globalramp);
        x->timing.recordhead = -1;
        *dirt = 1;
    } else if ((x->state.record - x->state.recordprev) > 0) { // samp @record-on
        x->fade.recordfade = x->fade.recfadeflag = 0;
        if (speed < 1.0)
            x->fade.snrfade = 0.0;
        if (x->fade.globalramp)
            kh_ease_bufoff(
                x->buffer.bframes - 1, b, x->buffer.nchans, accuratehead, -direction,
                x->fade.globalramp);
    }
}

// mono perform

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

                // interp ratio
                playhead = trunc(accuratehead);
                if (direction > 0) {
                    frac = accuratehead - playhead;
                } else if (direction < 0) {
                    frac = 1.0 - (accuratehead - playhead);
                } else {
                    frac = 0.0;
                } // setloopsize  // ??
                kh_interp_index(
                    playhead, &interp0, &interp1, &interp2, &interp3, direction,
                    directionorig, maxloop, frames - 1); // samp-indices

                // Perform playback interpolation
                osamp1 = kh_perform_playback_interpolation(
                    frac, b, interp0, interp1, interp2, interp3, pchans, interp, record);

                if (globalramp) { // "Switch and Ramp" -
                                  // http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
                    if (snrfade < 1.0) {
                        if (snrfade == 0.0) {
                            o1dif = o1prev - osamp1;
                        }
                        osamp1 += kh_ease_switchramp(
                            o1dif, snrfade, snrtype); // <- easing-curv options
                                                      // implemented by raja
                        snrfade += 1 / snrramp;
                    } // "Switch and Ramp" end

                    if (playfade < globalramp) { // realtime ramps for play on/off
                        osamp1 = kh_ease_record(
                            osamp1, (playfadeflag > 0), globalramp, playfade);
                        playfade++;
                        if (playfade >= globalramp) {
                            kh_process_playfade_state(
                                &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                                &playfade, &snrfade, record);
                        }
                    }
                } else {
                    kh_process_playfade_state(
                        &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                        &playfade, &snrfade, record);
                }

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
                // NOTUSED
                // if (direction > 0) {                            // interp
                // ratio
                //     //frac = accuratehead - playhead;
                // } else if (direction < 0) {
                //     frac = 1.0 - (accuratehead - playhead);
                // } else {
                //     frac = 0.0;
                // }

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

                // interp ratio
                playhead = trunc(accuratehead);
                if (direction > 0) {
                    frac = accuratehead - playhead;
                } else if (direction < 0) {
                    frac = 1.0 - (accuratehead - playhead);
                } else {
                    frac = 0.0;
                }
                kh_interp_index(
                    playhead, &interp0, &interp1, &interp2, &interp3, direction,
                    directionorig, maxloop, frames - 1); // samp-indices

                // Perform playback interpolation for both channels
                osamp1 = kh_perform_playback_interpolation(
                    frac, b, interp0, interp1, interp2, interp3, pchans, interp, record);
                osamp2 = (pchans > 1) ? kh_perform_playback_interpolation(
                    frac, b, interp0 + 1, interp1 + 1, interp2 + 1, interp3 + 1, pchans, interp, record) : osamp1;

                if (globalramp) { // "Switch and Ramp" -
                                  // http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
                    if (snrfade < 1.0) {
                        if (snrfade == 0.0) {
                            o1dif = o1prev - osamp1;
                            o2dif = o2prev - osamp2;
                        }
                        osamp1 += kh_ease_switchramp(
                            o1dif, snrfade, snrtype); // <- easing-curv options
                                                      // implemented by raja
                        osamp2 += kh_ease_switchramp(
                            o2dif, snrfade, snrtype);
                        snrfade += 1 / snrramp;
                    } // "Switch and Ramp" end

                    if (playfade < globalramp) { // realtime ramps for play on/off
                        osamp1 = kh_ease_record(
                            osamp1, (playfadeflag > 0), globalramp, playfade);
                        osamp2 = kh_ease_record(
                            osamp2, (playfadeflag > 0), globalramp, playfade);
                        playfade++;
                        if (playfade >= globalramp) {
                            kh_process_playfade_state(
                                &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                                &playfade, &snrfade, record);
                        }
                    }
                } else {
                    kh_process_playfade_state(
                        &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                        &playfade, &snrfade, record);
                }

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

void karma_poly_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr)
{
    long syncoutlet = x->syncoutlet;
    long nchans = x->buffer.ochans;

    // Safety check: ensure we don't exceed allocated memory
    if (nchans > x->poly_maxchans) {
        nchans = x->poly_maxchans;
    }

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
    double* osamp = x->poly_osamp;
    double* oprev = x->poly_oprev;
    double* odif = x->poly_odif;
    double* recin = x->poly_recin;

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
    for (i = 4; i < nchans; i++) {
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

                playhead = trunc(accuratehead);
                if (direction > 0) {
                    frac = accuratehead - playhead;
                } else if (direction < 0) {
                    frac = 1.0 - (accuratehead - playhead);
                } else {
                    frac = 0.0;
                }
                kh_interp_index(
                    playhead, &interp0, &interp1, &interp2, &interp3, direction,
                    directionorig, maxloop, frames - 1);

                for (i = 0; i < nchans; i++) {
                    long chan_offset = i % pchans;
                    osamp[i] = kh_perform_playback_interpolation(
                        frac, b + chan_offset, interp0 * pchans, interp1 * pchans,
                        interp2 * pchans, interp3 * pchans, pchans, interp, record);
                }

                if (globalramp) {
                    if (snrfade < 1.0) {
                        for (i = 0; i < nchans; i++) {
                            if (snrfade == 0.0) {
                                odif[i] = oprev[i] - osamp[i];
                            }
                            osamp[i] += kh_ease_switchramp(odif[i], snrfade, snrtype);
                        }
                        snrfade += 1 / snrramp;
                    }

                    if (playfade < globalramp) {
                        for (i = 0; i < nchans; i++) {
                            osamp[i] = kh_ease_record(
                                osamp[i], (playfadeflag > 0), globalramp, playfade);
                        }
                        playfade++;
                        if (playfade >= globalramp) {
                            kh_process_playfade_state(
                                &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                                &playfade, &snrfade, record);
                        }
                    }
                } else {
                    kh_process_playfade_state(
                        &playfadeflag, &go, &triginit, &jumpflag, &loopdetermine,
                        &playfade, &snrfade, record);
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
    t_buffer_obj* buf_temp;

    if (bufname == ps_nothing) {
        object_error((t_object*)x, "requires a valid buffer~ declaration (none found)");
        return false;
    }

    x->buffer.bufname_temp = bufname;

    if (!x->buffer.buf_temp) {
        x->buffer.buf_temp = buffer_ref_new((t_object*)x, bufname);
    } else {
        buffer_ref_set(x->buffer.buf_temp, bufname);
    }

    buf_temp = buffer_ref_getobject(x->buffer.buf_temp);

    if (buf_temp == NULL) {
        object_warn(
            (t_object*)x, "cannot find any buffer~ named %s, ignoring", bufname->s_name);
        x->buffer.buf_temp = 0;
        object_free(x->buffer.buf_temp);
        return false;
    }

    x->buffer.buf_temp = 0;
    object_free(x->buffer.buf_temp);

    // Set up the main buffer reference
    x->buffer.bufname = bufname;
    if (!x->buffer.buf) {
        x->buffer.buf = buffer_ref_new((t_object*)x, bufname);
    } else {
        buffer_ref_set(x->buffer.buf, bufname);
    }

    return true;
}

static inline void kh_parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag)
{
    if (loop_points_sym == ps_dummy) {
        *loop_points_flag = 2;
    } else if (
        (loop_points_sym == ps_phase) || (loop_points_sym == gensym("PHASE"))
                                      || (loop_points_sym == gensym("ph"))) {
        *loop_points_flag = 0;
    } else if (
        (loop_points_sym == ps_samples) || (loop_points_sym == gensym("SAMPLES"))
                                        || (loop_points_sym == gensym("samps"))) {
        *loop_points_flag = 1;
    } else if (
        (loop_points_sym == ps_milliseconds) || (loop_points_sym == gensym("MS"))
                                             || (loop_points_sym == gensym("ms"))) {
        *loop_points_flag = 2;
    } else {
        *loop_points_flag = 2; // default to milliseconds
    }
}

static inline void kh_parse_numeric_arg(t_atom* arg, double* value)
{
    if (atom_gettype(arg) == A_FLOAT) {
        *value = atom_getfloat(arg);
    } else if (atom_gettype(arg) == A_LONG) {
        *value = (double)atom_getlong(arg);
    }
}

static inline void kh_process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag)
{
    t_symbol* loop_points_sym = 0;
    double    temphightemp;

    // Initialize defaults
    *loop_points_flag = 2; // milliseconds
    *templow = -1.0;
    *temphigh = -1.0;

    // Process argument 4 (index 3) - loop points type
    if (argc >= 4) {
        if (atom_gettype(argv + 3) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 3);
            kh_parse_loop_points_sym(loop_points_sym, loop_points_flag);
        } else if (atom_gettype(argv + 3) == A_LONG) {
            *loop_points_flag = atom_getlong(argv + 3);
        } else if (atom_gettype(argv + 3) == A_FLOAT) {
            *loop_points_flag = (long)atom_getfloat(argv + 3);
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.4, using milliseconds "
                "for args 2 & 3",
                s->s_name);
            *loop_points_flag = 2;
        }
        *loop_points_flag = CLAMP(*loop_points_flag, 0, 2);
    }

    // Process argument 3 (index 2) - high value or loop points type
    if (argc >= 3) {
        if (atom_gettype(argv + 2) == A_FLOAT || atom_gettype(argv + 2) == A_LONG) {
            kh_parse_numeric_arg(argv + 2, temphigh);
            if (*temphigh < 0.) {
                object_warn(
                    (t_object*)x, "loop maximum cannot be less than 0., resetting");
            }
        } else if (atom_gettype(argv + 2) == A_SYM && argc < 4) {
            loop_points_sym = atom_getsym(argv + 2);
            kh_parse_loop_points_sym(loop_points_sym, loop_points_flag);
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.3, setting unit to "
                "maximum",
                s->s_name);
        }
    }

    // Process argument 2 (index 1) - low value or special handling
    if (argc >= 2) {
        if (atom_gettype(argv + 1) == A_FLOAT || atom_gettype(argv + 1) == A_LONG) {
            if (*temphigh < 0.) {
                temphightemp = *temphigh;
                kh_parse_numeric_arg(argv + 1, temphigh);
                *templow = temphightemp;
            } else {
                kh_parse_numeric_arg(argv + 1, templow);
                if (*templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    *templow = 0.;
                }
            }
        } else if (atom_gettype(argv + 1) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 1);
            if (loop_points_sym == ps_dummy) {
                *loop_points_flag = 2;
            } else if (loop_points_sym == ps_originalloop) {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand 'buffername' followed by "
                    "%s message, ignoring",
                    s->s_name, loop_points_sym->s_name);
                object_warn(
                    (t_object*)x,
                    "(the %s message cannot be used whilst changing buffer~ "
                    "reference",
                    loop_points_sym->s_name);
                object_warn(
                    (t_object*)x, "use %s %s message or just %s message instead)",
                    gensym("setloop")->s_name, ps_originalloop->s_name,
                    gensym("resetloop")->s_name);
                // Set flag to indicate early return needed
                *templow = -999.0; // Special flag value
                return;
            } else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.2, setting loop "
                    "points to minimum (and maximum)",
                    s->s_name);
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.2, setting loop points "
                "to defaults",
                s->s_name);
        }
    }
}

// Helper functions for further karma_mono_perform refactoring

void kh_process_ipoke_recording(
    float* b, long pchans, long playhead, long* recordhead, double recin1,
    double overdubamp, double globalramp, long recordfade, char recfadeflag,
    double* pokesteps, double* writeval1, t_bool* dirt)
{
    long   i;
    double recplaydif, coeff1;

    // Handle first record head initialization
    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = *writeval1;
        recplaydif = (double)(playhead - *recordhead);
        if (recplaydif > 0) { // linear-interpolation for speed > 1x
            coeff1 = (recin1 - *writeval1) / recplaydif;
            for (i = *recordhead + 1; i < playhead; i++) {
                *writeval1 += coeff1;
                b[i * pchans] = *writeval1;
            }
        } else {
            coeff1 = (recin1 - *writeval1) / recplaydif;
            for (i = *recordhead - 1; i > playhead; i--) {
                *writeval1 -= coeff1;
                b[i * pchans] = *writeval1;
            }
        }
        *writeval1 = recin1;
    }
    *recordhead = playhead;
    *dirt = 1;
}

static inline void kh_process_recording_fade(
    double globalramp, long* recordfade, char* recfadeflag, t_bool* record,
    t_bool* triginit, t_bool* jumpflag)
{
    if (globalramp) { // realtime ramps for record on/off
        if (*recordfade < globalramp) {
            (*recordfade)++;
            if ((*recfadeflag) && (*recordfade >= globalramp)) {
                if (*recfadeflag == 2) {
                    *triginit = *jumpflag = 1;
                    *recordfade = 0;
                } else if (*recfadeflag == 5) {
                    *record = 1;
                } else {
                    *record = 0;
                }
                *recfadeflag = 0;
            }
        }
    } else {
        if (*recfadeflag) {
            if (*recfadeflag == 2) {
                *triginit = *jumpflag = 1;
            } else if (*recfadeflag == 5) {
                *record = 1;
            } else {
                *record = 0;
            }
            *recfadeflag = 0;
        }
    }
}

static inline void kh_process_jump_logic(
    t_karma* x, float* b, double* accuratehead, t_bool* jumpflag, char direction)
{
    if (*jumpflag) { // jump
        if (x->state.directionorig >= 0) {
            *accuratehead = x->timing.jumphead * x->timing.maxhead; // !! maxhead !!
        } else {
            *accuratehead = (x->buffer.bframes - 1)
                - (((x->buffer.bframes - 1) - x->timing.maxhead) * x->timing.jumphead);
        }
        *jumpflag = 0;
        x->fade.snrfade = 0.0;
        if (x->state.record) {
            if (x->fade.globalramp) {
                kh_ease_bufon(
                    x->buffer.bframes - 1, b, x->buffer.nchans, *accuratehead,
                    x->timing.recordhead, direction, x->fade.globalramp);
                x->fade.recordfade = 0;
            }
            x->fade.recfadeflag = 0;
            x->timing.recordhead = -1;
        }
        x->state.triginit = 0;
    }
}

static inline void kh_process_initial_loop_ipoke_recording(
    float* b, long pchans, long* recordhead, long playhead, double recin1,
    double* pokesteps, double* writeval1, char direction, char directionorig,
    long maxhead, long frames)
{
    long   i;
    double recplaydif, coeff1;

    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = *writeval1;
        recplaydif = (double)(playhead - *recordhead); // linear-interp for speed > 1x

        if (direction != directionorig) {
            if (directionorig >= 0) {
                if (recplaydif > 0) {
                    if (recplaydif > (maxhead * 0.5)) {
                        recplaydif -= maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i >= 0; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = *writeval1;
                        }
                        kh_apply_ipoke_interpolation(
                            b, pchans, maxhead, playhead, writeval1, coeff1, -1);
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    }
                } else {
                    if ((-recplaydif) > (maxhead * 0.5)) {
                        recplaydif += maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < (maxhead + 1); i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = *writeval1;
                        }
                        for (i = 0; i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    }
                }
            } else {
                if (recplaydif > 0) {
                    if (recplaydif > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif -= ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i >= maxhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = *writeval1;
                        }
                        for (i = (frames - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    }
                } else {
                    if ((-recplaydif) > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif += ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < frames; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = *writeval1;
                        }
                        kh_apply_ipoke_interpolation(
                            b, pchans, maxhead, playhead, writeval1, coeff1, 1);
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = *writeval1;
                        }
                    }
                }
            }
        } else {
            if (recplaydif > 0) {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                for (i = (*recordhead + 1); i < playhead; i++) {
                    *writeval1 += coeff1;
                    b[i * pchans] = *writeval1;
                }
            } else {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                for (i = (*recordhead - 1); i > playhead; i--) {
                    *writeval1 -= coeff1;
                    b[i * pchans] = *writeval1;
                }
            }
        }
        *writeval1 = recin1;
    }
}

static inline void kh_process_initial_loop_boundary_constraints(
    t_karma* x, float* b, double* accuratehead, double speed, char direction)
{
    long   setloopsize;
    double speedsrscaled;

    setloopsize = x->loop.maxloop
        - x->loop.minloop; // not really required here because initial loop ??
    speedsrscaled = speed * x->timing.srscale;
    if (x->state.record) // why 1024 ??
        speedsrscaled = (fabs(speedsrscaled) > (setloopsize / 1024))
            ? ((setloopsize / 1024) * direction)
            : speedsrscaled;
    *accuratehead = *accuratehead + speedsrscaled;

    if (direction
        == x->state.directionorig) { // buffer~ boundary constraints and registry of
                                     // maximum distance traversed
        if (*accuratehead > (x->buffer.bframes - 1)) {
            *accuratehead = 0.0;
            x->state.record = x->state.append;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    kh_ease_bufoff(
                        x->buffer.bframes - 1, b, x->buffer.nchans,
                        (x->buffer.bframes - 1), -direction,
                        x->fade.globalramp); // maxloop ??
                    x->timing.recordhead = -1;
                    x->fade.recfadeflag = x->fade.recordfade = 0;
                }
            }
            x->state.recendmark = x->state.triginit = 1;
            x->state.loopdetermine = x->state.alternateflag = 0;
            x->timing.maxhead = x->buffer.bframes - 1;
        } else if (*accuratehead < 0.0) {
            *accuratehead = x->buffer.bframes - 1;
            x->state.record = x->state.append;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    kh_ease_bufoff(
                        x->buffer.bframes - 1, b, x->buffer.nchans, x->loop.minloop,
                        -direction,
                        x->fade.globalramp); // 0.0  // ??
                    x->timing.recordhead = -1;
                    x->fade.recfadeflag = x->fade.recordfade = 0;
                }
            }
            x->state.recendmark = x->state.triginit = 1;
            x->state.loopdetermine = x->state.alternateflag = 0;
            x->timing.maxhead = 0.0;
        } else { // <- track max write position
            if (((x->state.directionorig >= 0) && (x->timing.maxhead < *accuratehead))
                || ((x->state.directionorig < 0)
                    && (x->timing.maxhead > *accuratehead))) {
                x->timing.maxhead = *accuratehead;
            }
        }
    } else if (direction < 0) { // wraparounds for reversal while creating initial-loop
        if (*accuratehead < 0.0) {
            *accuratehead = x->timing.maxhead + *accuratehead;
            if (x->fade.globalramp) {
                kh_ease_bufoff(
                    x->buffer.bframes - 1, b, x->buffer.nchans, x->loop.minloop,
                    -direction, x->fade.globalramp); // 0.0  // ??
                x->timing.recordhead = -1;
                x->fade.recfadeflag = x->fade.recordfade = 0;
            }
        }
    } else if (direction >= 0) {
        if (*accuratehead > (x->buffer.bframes - 1)) {
            *accuratehead = x->timing.maxhead + (*accuratehead - (x->buffer.bframes - 1));
            if (x->fade.globalramp) {
                kh_ease_bufoff(
                    x->buffer.bframes - 1, b, x->buffer.nchans, (x->buffer.bframes - 1),
                    -direction,
                    x->fade.globalramp); // maxloop ??
                x->timing.recordhead = -1;
                x->fade.recfadeflag = x->fade.recordfade = 0;
            }
        }
    }
}

static inline double kh_process_audio_interpolation(
    float* b, long pchans, double accuratehead, interp_type_t interp, t_bool record)
{
    long   playhead = (long)accuratehead;
    double frac = accuratehead - playhead;
    double output = 0.0;

    if (!record) { // if recording do linear-interp else...
        switch (interp) {
        case INTERP_CUBIC:
            // Cubic interpolation would go here
            output = (double)b[playhead * pchans];
            break;
        default: // INTERP_LINEAR
            if (frac > 0.0) {
                output = ((double)b[playhead * pchans] * (1.0 - frac))
                    + ((double)b[(playhead + 1) * pchans] * frac);
            } else {
                output = (double)b[playhead * pchans];
            }
            break;
        }
    } else {
        output = (double)b[playhead * pchans];
    }

    return output;
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
