// karma_re~ : a thin Max/MSP shell over the host-agnostic karma_core DSP.
//
// All looping/recording/playback logic lives in karma_core (../karma_core).
// This file owns only the Max-specific plumbing: object lifecycle, inlets/
// outlets, the buffer~ reference, the report clock, and attributes. buffer~
// access is handed to the core through a karma_buffer_iface; control messages
// and the per-vector perform call are forwarded to the core.
//
// Scope note: covers transport (record/play/stop/overdub/append/jump), speed,
// selection (position/window), loop points (setloop/resetloop), and buffer
// association (set). Still unimplemented from the reference: 'multiply' and
// 'offset' (not extracted into the core yet).

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include "karma_core_api.h"   // the Max-free core (after the c74 headers)

// ---------------------------------------------------------------------------
typedef struct _karma_re {
    t_pxobject     ob;
    t_karma        core;          // the DSP engine (by value)

    t_buffer_ref  *buf;
    t_symbol      *bufname;
    void          *messout;       // data/list outlet
    void          *tclock;        // report clock
    t_bool         buf_modified;
    t_bool         clockgo;       // shell-owned report-clock arming flag. NOT core.clockgo:
                                  // the core's verbatim perform tail still toggles its own
                                  // clockgo (with no-op clock shims), so the shell must keep
                                  // its own or the report clock never arms during playback.
    long           syncoutlet;    // @syncout attribute (instantiation-time)
    long           reportlist;    // report interval in ms
} t_karma_re;

static t_class  *karma_re_class = NULL;
static t_symbol *ps_buffer_modified;
static t_symbol *ps_phase, *ps_samples, *ps_milliseconds, *ps_reset;

// ---------------------------------------------------------------------------
// buffer~ access exposed to the core through the host interface
// ---------------------------------------------------------------------------
static void *kre_buf_lock(void *ctx)
{
    t_karma_re *x = (t_karma_re *)ctx;
    t_buffer_obj *b = buffer_ref_getobject(x->buf);
    return b ? buffer_locksamples(b) : NULL;
}
static void kre_buf_unlock(void *ctx)
{
    t_karma_re *x = (t_karma_re *)ctx;
    t_buffer_obj *b = buffer_ref_getobject(x->buf);
    if (b) buffer_unlocksamples(b);
}
static void kre_buf_setdirty(void *ctx)
{
    t_karma_re *x = (t_karma_re *)ctx;
    t_buffer_obj *b = buffer_ref_getobject(x->buf);
    if (b) buffer_setdirty(b);
}

// Point the core at our buffer~ and (re)query its dimensions.
static void kre_buf_setup(t_karma_re *x, t_symbol *s)
{
    x->bufname = s;
    if (!x->buf)
        x->buf = buffer_ref_new((t_object *)x, s);
    else
        buffer_ref_set(x->buf, s);

    t_buffer_obj *b = buffer_ref_getobject(x->buf);
    if (!b) { x->buf = 0; return; }

    x->core.bufio.lock      = kre_buf_lock;
    x->core.bufio.unlock    = kre_buf_unlock;
    x->core.bufio.set_dirty = kre_buf_setdirty;
    x->core.bufio.ctx       = x;
    x->core.bufio.frames    = (long)buffer_getframecount(b);
    x->core.bufio.chans     = (long)buffer_getchannelcount(b);
    x->core.bufio.sr        = buffer_getsamplerate(b);
    karma_core_set_dims(&x->core);
}

// ---------------------------------------------------------------------------
// report list outlet (ported from the reference karma_clock_list)
// ---------------------------------------------------------------------------
void karma_re_clock_list(t_karma_re *x)
{
    if (x->reportlist <= 0) return;

    t_karma *c = &x->core;
    long   frames      = (long)c->bframes - 1;
    long   setloopsize = (long)(c->maxloop - c->minloop);
    double bmsr        = c->bmsr;
    t_bool directflag  = c->directionorig < 0;

    double normpos = CLAMP(directflag
        ? ((c->playhead - (frames - setloopsize)) / (double)setloopsize)
        : ((c->playhead - c->minloop) / (double)setloopsize), 0., 1.);

    double startms = (directflag ? (double)(frames - setloopsize) : (double)c->minloop) / bmsr;
    double endms   = (directflag ? (double)frames                : (double)c->maxloop) / bmsr;
    double winms   = (c->selection * (double)setloopsize) / bmsr;

    t_atom dl[7];
    atom_setfloat(dl + 0, normpos);
    atom_setlong (dl + 1, c->go);
    atom_setlong (dl + 2, c->record);
    atom_setfloat(dl + 3, startms);
    atom_setfloat(dl + 4, endms);
    atom_setfloat(dl + 5, winms);
    atom_setlong (dl + 6, c->statehuman);
    outlet_list(x->messout, 0L, 7, dl);

    if (sys_getdspstate())
        clock_delay(x->tclock, x->reportlist);
}

// ---------------------------------------------------------------------------
// control messages -> core
// ---------------------------------------------------------------------------
void karma_re_record(t_karma_re *x)               { karma_record(&x->core); x->clockgo = 1; }
void karma_re_play(t_karma_re *x)                 { karma_play(&x->core);   x->clockgo = 1; }
void karma_re_stop(t_karma_re *x)                 { karma_stop(&x->core); }
void karma_re_append(t_karma_re *x)               { karma_append(&x->core); }
void karma_re_overdub(t_karma_re *x, double amp)  { karma_overdub(&x->core, amp); }
void karma_re_jump(t_karma_re *x, double pos)     { karma_jump(&x->core, pos); }
void karma_re_position(t_karma_re *x, double p)   { karma_select_start(&x->core, p); }
void karma_re_window(t_karma_re *x, double w)     { karma_select_size(&x->core, w); }

// 'speed' float arrives on the last inlet; mirror the reference gate.
void karma_re_float(t_karma_re *x, double f)
{
    long inlet = proxy_getinlet((t_object *)x);
    if (inlet == x->core.ochans)
        x->core.speedfloat = f;
}

// map a unit symbol to the core's points_flag (0 phase / 1 samples / 2 ms)
static long kre_units_flag(t_symbol *u, long deflt)
{
    if (u == ps_phase   || u == gensym("PHASE")   || u == gensym("ph"))    return 0;
    if (u == ps_samples || u == gensym("SAMPLES") || u == gensym("samps")) return 1;
    if (u == ps_milliseconds || u == gensym("MS") || u == gensym("ms"))    return 2;
    return deflt;
}

// "setloop [low] [high] [units]" parsing, ported from the reference; the loop
// maths itself is delegated to karma_core_set_loop.
static void karma_re_setloop_internal(t_karma_re *x, t_symbol *s, short argc, t_atom *argv)
{
    long   flag = 2;                 // default milliseconds
    double templow = -1., temphigh = -1., temphightemp;

    if (argc >= 3) {
        if (argc > 3)
            object_warn((t_object *)x, "too many arguments for %s, truncating to first three", s->s_name);
        long t = atom_gettype(argv + 2);
        if (t == A_SYM)        flag = kre_units_flag(atom_getsym(argv + 2), 2);
        else if (t == A_LONG)  flag = (long)atom_getlong(argv + 2);
        else if (t == A_FLOAT) flag = (long)atom_getfloat(argv + 2);
        else                   flag = 2;
        flag = CLAMP(flag, 0, 2);
    }
    if (argc >= 2) {
        long t = atom_gettype(argv + 1);
        if (t == A_FLOAT)      temphigh = atom_getfloat(argv + 1);
        else if (t == A_LONG)  temphigh = (double)atom_getlong(argv + 1);
        else if (t == A_SYM && argc < 3) flag = kre_units_flag(atom_getsym(argv + 1), 2);
    }
    if (argc >= 1) {
        long t = atom_gettype(argv + 0);
        if (t == A_FLOAT || t == A_LONG) {
            double v = (t == A_FLOAT) ? atom_getfloat(argv + 0) : (double)atom_getlong(argv + 0);
            if (temphigh < 0.) { temphightemp = temphigh; temphigh = v; templow = temphightemp; }
            else               { templow = (v < 0.) ? 0. : v; }
        }
    }
    karma_core_set_loop(&x->core, templow, temphigh, flag);
}

void karma_re_setloop(t_karma_re *x, t_symbol *s, short ac, t_atom *av)
{
    if (ac == 1 && atom_gettype(av) == A_SYM) {
        if (atom_getsym(av) == ps_reset)
            karma_core_set_loop(&x->core, (double)x->core.initiallow, (double)x->core.initialhigh, 1);
        else
            object_error((t_object *)x, "%s does not understand %s", s->s_name, atom_getsym(av)->s_name);
    } else {
        karma_re_setloop_internal(x, s, ac, av);
    }
}

void karma_re_resetloop(t_karma_re *x)
{
    karma_core_set_loop(&x->core, (double)x->core.initiallow, (double)x->core.initialhigh, 1);
}

// associate / change the buffer~ (resets the loop window to the new buffer)
void karma_re_set(t_karma_re *x, t_symbol *s, short ac, t_atom *av)
{
    if (ac < 1 || atom_gettype(av) != A_SYM) {
        object_error((t_object *)x, "%s requires a buffer~ name", s->s_name);
        return;
    }
    t_symbol *name = atom_getsym(av);
    kre_buf_setup(x, name);
    if (!x->buf)
        object_warn((t_object *)x, "set: no buffer~ named %s", name->s_name);
}

// ---------------------------------------------------------------------------
// perform wrappers -> core (the core locks/unlocks the buffer via the iface)
// ---------------------------------------------------------------------------
void karma_re_mono_perform(t_karma_re *x, t_object *dsp64, double **ins, long nins,
                           double **outs, long nouts, long vcount, long flags, void *usr)
{
    karma_mono_perform(&x->core, dsp64, ins, nins, outs, nouts, vcount, flags, usr);
    if (x->clockgo) { clock_delay(x->tclock, 0); x->clockgo = 0; }
    else if (!x->core.go || x->reportlist <= 0) { clock_unset(x->tclock); x->clockgo = 1; }
}
void karma_re_stereo_perform(t_karma_re *x, t_object *dsp64, double **ins, long nins,
                             double **outs, long nouts, long vcount, long flags, void *usr)
{
    karma_stereo_perform(&x->core, dsp64, ins, nins, outs, nouts, vcount, flags, usr);
    if (x->clockgo) { clock_delay(x->tclock, 0); x->clockgo = 0; }
    else if (!x->core.go || x->reportlist <= 0) { clock_unset(x->tclock); x->clockgo = 1; }
}
void karma_re_quad_perform(t_karma_re *x, t_object *dsp64, double **ins, long nins,
                           double **outs, long nouts, long vcount, long flags, void *usr)
{
    karma_quad_perform(&x->core, dsp64, ins, nins, outs, nouts, vcount, flags, usr);
    if (x->clockgo) { clock_delay(x->tclock, 0); x->clockgo = 0; }
    else if (!x->core.go || x->reportlist <= 0) { clock_unset(x->tclock); x->clockgo = 1; }
}

// ---------------------------------------------------------------------------
// dsp / buffer / lifecycle
// ---------------------------------------------------------------------------
void karma_re_dsp64(t_karma_re *x, t_object *dsp64, short *count, double srate, long vecount, long flags)
{
    x->core.ssr    = srate;
    x->core.vs     = (double)vecount;
    x->core.vsnorm = (double)vecount / srate;
    x->clockgo = 1;

    if (x->bufname) {
        kre_buf_setup(x, x->bufname);
        x->core.syncoutlet  = x->syncoutlet;

        long ochans = (long)x->core.ochans;
        x->core.speedconnect = ochans <= 1 ? count[1] : (ochans == 2 ? count[2] : count[4]);

        method perf = (ochans <= 1) ? (method)karma_re_mono_perform
                    : (ochans == 2) ? (method)karma_re_stereo_perform
                                    : (method)karma_re_quad_perform;
        object_method(dsp64, gensym("dsp_add64"), x, perf, 0, NULL);

        // First DSP-on enables the transport gate the reference sets in its own
        // dsp64 (karma~.c): without it karma_stop / karma_jump stay gated off.
        // The loop bounds are already initialised by kre_buf_setup above.
        x->core.initinit = 1;
    }
}

void karma_re_buf_dblclick(t_karma_re *x)
{
    buffer_view(buffer_ref_getobject(x->buf));
}

t_max_err karma_re_notify(t_karma_re *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    if (msg == ps_buffer_modified)
        x->buf_modified = true;
    return buffer_ref_notify(x->buf, s, msg, sender, data);
}

void karma_re_assist(t_karma_re *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET)
        snprintf_zero(s, 256, (a == 0) ? "(signal) Record Input / messages"
                                       : "(signal/float) Speed (1 = normal)");
    else
        snprintf_zero(s, 256, "(signal) Audio Output");
}

void *karma_re_new(t_symbol *s, short argc, t_atom *argv)
{
    t_karma_re *x = (t_karma_re *)object_alloc(karma_re_class);
    if (!x) return NULL;

    t_symbol *bufname = (argc > 0) ? atom_getsym(argv) : 0;
    long chans = (argc > 1) ? (long)atom_getlong(argv + 1) : 1;
    chans = (chans <= 1) ? 1 : (chans == 2 ? 2 : 4);

    dsp_setup((t_pxobject *)x, (chans <= 1) ? 2 : (chans == 2 ? 3 : 5));

    karma_core_init(&x->core, chans, sys_getsr(), sys_getblksize());
    x->bufname    = bufname;
    x->reportlist = 50;
    x->buf        = 0;

    x->messout = listout(x);
    x->tclock  = clock_new((t_object *)x, (method)karma_re_clock_list);
    attr_args_process(x, argc, argv);

    long n = (x->syncoutlet ? 1 : 0) + chans;
    for (long i = 0; i < n; i++) outlet_new(x, "signal");

    x->ob.z_misc |= Z_NO_INPLACE;
    return x;
}

void karma_re_free(t_karma_re *x)
{
    dsp_free((t_pxobject *)x);
    if (x->buf)     object_free(x->buf);
    if (x->tclock)  object_free(x->tclock);
}

// ---------------------------------------------------------------------------
void ext_main(void *r)
{
    t_class *c = class_new("karma_re~", (method)karma_re_new, (method)karma_re_free,
                           (long)sizeof(t_karma_re), 0L, A_GIMME, 0);

    class_addmethod(c, (method)karma_re_record,   "record",            0);
    class_addmethod(c, (method)karma_re_play,     "play",              0);
    class_addmethod(c, (method)karma_re_stop,     "stop",              0);
    class_addmethod(c, (method)karma_re_append,   "append",            0);
    class_addmethod(c, (method)karma_re_overdub,  "overdub",  A_FLOAT, 0);
    class_addmethod(c, (method)karma_re_jump,     "jump",     A_FLOAT, 0);
    class_addmethod(c, (method)karma_re_position, "position", A_FLOAT, 0);
    class_addmethod(c, (method)karma_re_window,   "window",   A_FLOAT, 0);
    class_addmethod(c, (method)karma_re_float,    "float",    A_FLOAT, 0);
    class_addmethod(c, (method)karma_re_setloop,  "setloop",  A_GIMME, 0);
    class_addmethod(c, (method)karma_re_resetloop,"resetloop",         0);
    class_addmethod(c, (method)karma_re_set,      "set",      A_GIMME, 0);

    class_addmethod(c, (method)karma_re_dsp64,       "dsp64",     A_CANT, 0);
    class_addmethod(c, (method)karma_re_assist,      "assist",    A_CANT, 0);
    class_addmethod(c, (method)karma_re_buf_dblclick,"dblclick",  A_CANT, 0);
    class_addmethod(c, (method)karma_re_notify,      "notify",    A_CANT, 0);

    // syncout must be known at instantiation (it adds an outlet) -> shell field.
    CLASS_ATTR_LONG(c, "syncout", 0, t_karma_re, syncoutlet);
    CLASS_ATTR_INVISIBLE(c, "syncout", 0);
    CLASS_ATTR_LABEL(c, "syncout", 0, "Create audio-rate Sync Outlet 0/1");

    CLASS_ATTR_LONG(c, "report", 0, t_karma_re, reportlist);
    CLASS_ATTR_FILTER_MIN(c, "report", 0);
    CLASS_ATTR_LABEL(c, "report", 0, "Report Time (ms) for data outlet");

    // DSP params live in the core and are read by the perform routines, so map
    // these attributes directly onto the embedded core fields.
    CLASS_ATTR_LONG(c, "ramp", 0, t_karma_re, core.globalramp);
    CLASS_ATTR_FILTER_CLIP(c, "ramp", 0, 2048);
    CLASS_ATTR_LABEL(c, "ramp", 0, "Ramp Time (samples)");

    CLASS_ATTR_LONG(c, "snramp", 0, t_karma_re, core.snrramp);
    CLASS_ATTR_FILTER_CLIP(c, "snramp", 0, 2048);
    CLASS_ATTR_LABEL(c, "snramp", 0, "Switch&Ramp Time (samples)");

    CLASS_ATTR_LONG(c, "snrcurv", 0, t_karma_re, core.snrtype);
    CLASS_ATTR_FILTER_CLIP(c, "snrcurv", 0, 6);
    CLASS_ATTR_ENUMINDEX(c, "snrcurv", 0, "Linear Sine_In Cubic_In Cubic_Out Exp_In Exp_Out Exp_In_Out");
    CLASS_ATTR_LABEL(c, "snrcurv", 0, "Switch&Ramp Curve");

    CLASS_ATTR_LONG(c, "interp", 0, t_karma_re, core.interpflag);
    CLASS_ATTR_FILTER_CLIP(c, "interp", 0, 2);
    CLASS_ATTR_ENUMINDEX(c, "interp", 0, "Linear Cubic Spline");
    CLASS_ATTR_LABEL(c, "interp", 0, "Playback Interpolation");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    karma_re_class = c;

    ps_buffer_modified = gensym("buffer_modified");
    ps_phase           = gensym("phase");
    ps_samples         = gensym("samples");
    ps_milliseconds    = gensym("milliseconds");
    ps_reset           = gensym("reset");
}
