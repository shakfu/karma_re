// clang-format off
struct t_karma {
    
    t_pxobject      k_ob;
    
    // Buffer management group
    struct {
        t_buffer_ref    *buf;
        t_buffer_ref    *buf_temp;      // so that 'set' errors etc do not interupt current buf playback ...
        t_symbol        *bufname;
        t_symbol        *bufname_temp;  // ...
        long   bframes;         // number of buffer frames (number of floats long the buffer is for a single channel)
        long   bchans;          // number of buffer channels (number of floats in a frame, stereo has 2 samples per frame, etc.)
        long   ochans;          // number of object audio channels (object arg #2: 1 / 2 / 4)
        long   nchans;          // number of channels to actually address (use only channel one if 'ochans' == 1, etc.)
    } buffer;

    // Timing and sample rate group
    struct {
        double  ssr;            // system samplerate
        double  bsr;            // buffer samplerate
        double  bmsr;           // buffer samplerate in samples-per-millisecond
        double  srscale;        // scaling factor: buffer samplerate / system samplerate ("to scale playback speeds appropriately")
        double  vs;             // system vectorsize
        double  vsnorm;         // normalised system vectorsize
        double  bvsnorm;        // normalised buffer vectorsize
        double  playhead;       // play position in samples (raja: "double so that capable of tracking playhead position in floating-point indices")
        double  maxhead;        // maximum playhead position that the recording has gone into the buffer~ in samples  // ditto
        double  jumphead;       // jump position (in terms of phase 0..1 of loop) <<-- of 'loop', not 'buffer~'
        long   recordhead;      // record head position in samples
        double  selstart;       // start position of window ('selection') within loop set by the 'position $1' message sent to object (in phase 0..1)
        double  selection;      // selection length of window ('selection') within loop set by 'window $1' message sent to object (in phase 0..1)
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

//  double  selmultiply;    // store loop length multiplier amount from 'multiply' method -->> TODO
    double  speedfloat;     // store speed inlet value if float (not signal)
    long    syncoutlet;     // make sync outlet ? (object attribute @syncout, instantiation time only)
//  long    boffset;        // zero indexed buffer channel # (default 0), user settable, not buffer~ queried -->> TODO
    long    moduloout;      // modulo playback channel outputs flag, user settable, not buffer~ queried -->> TODO
    long    islooped;       // can disable/enable global looping status (rodrigo @ttribute request, TODO) (!! long ??)
    short   speedconnect;   // 'count[]' info for 'speed' as signal or float in perform routines
    long   reportlist;      // right list outlet report granularity in ms (!! why is this a long ??)

    void    *messout;       // list outlet pointer
    void    *tclock;        // list timer pointer
};