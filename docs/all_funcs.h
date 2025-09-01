
// static inline easing functions
double linear_interp(double f, double x, double y);
double cubic_interp(double f, double w, double x, double y, double z);
double spline_interp(double f, double w, double x, double y, double z);
double ease_record(double y1, char updwn, double globalramp, long playfade);
double ease_switchramp(double y1, double snrfade, switchramp_type_t snrtype);
void ease_bufoff(long framesm1, float *buf, long pchans, long markposition, char direction, double globalramp);
void apply_fade(long pos, long framesm1, float *buf, long pchans, double fade);
void ease_bufon(long framesm1, float *buf, long pchans, long markposition1, long markposition2, char direction, double globalramp);

// Helper function to handle recording fade completion logic
void handle_recording_fade_completion(char recfadeflag, char *recendmark, t_bool *record, t_bool *triginit, t_bool *jumpflag, t_bool *loopdetermine, long *recordfade, char directionorig, long *maxloop, long maxhead, long frames)

// Helper function to calculate sync outlet output
void calculate_sync_output(double osamp1, double *o1prev, double **out1, char syncoutlet, double **outPh, double accuratehead, double minloop, double maxloop, char directionorig, long frames, double setloopsize)

// Helper function to apply iPoke interpolation over a range
void apply_ipoke_interpolation(float *b, long pchans, long start_idx, long end_idx, double *writeval1, double coeff1, char direction) 

// Helper function to initialize buffer properties
void init_buffer_properties(t_karma *x, t_buffer_obj *buf);

// Helpers for handle_loop_boundary
void handle_recording_cleanup(
    t_bool record, double globalramp, long frames, float *b, long pchans,
    double accuratehead, long *recordhead, char direction, long *recordfade, 
    char *recfadeflag, double *snrfade, t_bool use_ease_on, double ease_pos);

void handle_forward_jump_boundary(
    double *accuratehead, long maxloop, long setloopsize, t_bool record,
    double globalramp, long frames, float *b, long pchans, long *recordhead,
    char direction, long *recordfade, char *recfadeflag, double *snrfade);

void handle_reverse_jump_boundary(
    double *accuratehead, long frames, long setloopsize, long maxloop,
    t_bool record, double globalramp, float *b, long pchans, long *recordhead,
    char direction, long *recordfade, char *recfadeflag, double *snrfade);

void handle_forward_wrap_boundary(
    double *accuratehead, long maxloop, long minloop, long setloopsize,
    t_bool record, double globalramp, long frames, float *b, long pchans,
    long *recordhead, char direction, long *recordfade, char *recfadeflag,
    double *snrfade);

void handle_reverse_wrap_boundary(
    double *accuratehead, long frames, long maxloop, long setloopsize,
    t_bool record, double globalramp, float *b, long pchans, long *recordhead,
    char direction, long *recordfade, char *recfadeflag, double *snrfade);

// Helper function to handle loop boundary wrapping and jumping
void handle_loop_boundary(
    double *accuratehead, double speed, double srscale, char direction, 
    char directionorig, long frames, long maxloop, long minloop, 
    long setloopsize, long startloop, long endloop, t_bool wrapflag, 
    t_bool jumpflag, t_bool record, double globalramp, float *b, 
    long pchans, long *recordhead, long *recordfade, char *recfadeflag,
    double *snrfade);

// Helper function to perform playback interpolation
double perform_playback_interpolation(
    double frac, float *b, long interp0, long interp1, 
    long interp2, long interp3, long pchans, 
    interp_type_t interp, t_bool record)

// Helper function to handle playfade state machine logic
void handle_playfade_state(
    char *playfadeflag, t_bool *go, t_bool *triginit, t_bool *jumpflag, 
    t_bool *loopdetermine, long *playfade, double *snrfade, t_bool record)

// Helper function to handle loop initialization and calculation
void handle_loop_initialization(
    t_bool triginit, char recendmark, char directionorig, 
    long *maxloop, long minloop, long maxhead, long frames, 
    double selstart, double selection, double *accuratehead, 
    long *startloop, long *endloop, long *setloopsize, 
    t_bool *wrapflag, char direction, double globalramp, 
    float *b, long pchans, long recordhead, t_bool record, 
    t_bool jumpflag, double jumphead, double *snrfade, 
    t_bool *append, t_bool *alternateflag, char *recendmark_ptr)

// Helper function to handle initial loop creation state
void handle_initial_loop_creation(
    t_bool go, t_bool triginit, t_bool jumpflag, t_bool append, 
    double jumphead, long maxhead, long frames, char directionorig, 
    double *accuratehead, double *snrfade, t_bool record, 
    double globalramp, float *b, long pchans, long *recordhead, 
    char direction, long *recordfade, char *recfadeflag, 
    t_bool *alternateflag, t_bool *triginit_ptr)

// Helper to wrap index for forward or reverse looping
long wrap_index(long idx, char directionorig, long maxloop, long framesm1)


void interp_index(long playhead, long *indx0, long *indx1, long *indx2, long *indx3, char direction, char directionorig, long maxloop, long framesm1)

void ext_main(void *r)
void* karma_new(t_symbol* s, short argc, t_atom* argv)
void karma_free(t_karma* x)
void karma_buf_dblclick(t_karma* x)
void karma_buf_setup(t_karma* x, t_symbol* s)

// called on buffer modified notification
void karma_buf_modify(t_karma* x, t_buffer_obj* b)
void karma_buf_values_internal(t_karma* x, double templow, double temphigh, long loop_points_flag, t_bool caller)
void karma_buf_change_internal(t_karma* x, t_symbol* s, short argc, t_atom* argv)
void karma_buf_change(t_karma* x, t_symbol* s, short ac, t_atom* av)
void karma_setloop_internal(t_karma* x, t_symbol* s, short argc, t_atom* argv)
void karma_setloop(t_karma* x, t_symbol* s, short ac, t_atom* av)
void karma_resetloop(t_karma* x)
void karma_clock_list(t_karma* x)
void karma_assist(t_karma* x, void* b, long m, long a, char* s)
void karma_float(t_karma* x, double speedfloat)
void karma_select_start(t_karma* x, double positionstart)
void karma_select_size(t_karma* x, double duration)
void karma_stop(t_karma* x)
void karma_play(t_karma* x)
// Helper to clear buffer
t_bool _clear_buffer(t_buffer_obj* buf, long bframes, long rchans)
void karma_record(t_karma* x)
void karma_append(t_karma* x)
void karma_overdub(t_karma* x, double amplitude)
void karma_jump(t_karma* x, double jumpposition)
t_max_err karma_syncout_set(t_karma* x, t_object* attr, long argc, t_atom* argv)
t_max_err karma_buf_notify(t_karma* x, t_symbol* s, t_symbol* msg, void* sndr, void* dat)
void karma_dsp64(t_karma* x, t_object* dsp64, short* count, double srate, long vecount, long flags)

// Helper function: Process state control switch statement
void karma_process_state_control(
	t_karma* x, control_state_t* statecontrol, t_bool* record, t_bool* go,
    t_bool* triginit, t_bool* loopdetermine, long* recordfade, char* recfadeflag,
    long* playfade, char* playfadeflag, char* recendmark)

// Helper function: Initialize performance variables
void karma_initialize_perform_vars(
    t_karma* x, double* accuratehead, long* playhead, double* maxhead, t_bool* wrapflag,
    double* jumphead, double* pokesteps, double* snrfade, double* globalramp,
    double* snrramp, switchramp_type_t* snrtype, interp_type_t* interp,
    double* speedfloat, double* o1prev, double* o1dif, double* writeval1)

// Helper function: Handle direction changes
void karma_handle_direction_change(
    char directionprev, char direction, t_bool record, double globalramp, long frames,
    float* b, long pchans, long recordhead, long* recordfade, char* recfadeflag,
    double* snrfade)

// Helper function: Handle record on/off transitions
void karma_handle_record_toggle(
    t_bool record, t_bool recordprev, double globalramp, long frames, float* b,
    long pchans, long* recordhead, long* recordfade, char* recfadeflag,
    double accuratehead, char direction, double speed, double* snrfade, t_bool* dirt)

void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs, long nouts,
    long vcount, long flgs, void* usr)


// ============================== Helper Functions

// Helper functions for karma_buf_change_internal refactoring

t_bool karma_validate_buffer(t_karma* x, t_symbol* bufname)
void karma_parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag)
void karma_parse_numeric_arg(t_atom* arg, double* value)
void karma_process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag)

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

void handle_initial_loop_ipoke_recording(
    float* b, long pchans, long* recordhead, long playhead, double recin1,
    double* pokesteps, double* writeval1, char direction, char directionorig,
    long maxhead, long frames);

void handle_initial_loop_boundary_constraints(
    double* accuratehead, double speed, double srscale, char direction, char directionorig,
    long frames, long maxloop, long minloop, t_bool append, t_bool* record,
    double globalramp, float* b, long pchans, long* recordhead, char* recfadeflag,
    long* recordfade, char* recendmark, t_bool* triginit, t_bool* loopdetermine,
    t_bool* alternateflag, double* maxhead)

double karma_process_audio_interpolation(
    float* b, long pchans, double accuratehead, interp_type_t interp, t_bool record)

