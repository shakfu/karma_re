// karma_core : the Max-free DSP core extracted from karma~.c.
//
// Field names and the float math are copied verbatim from the reference so the
// behaviour is identical (validated sample-for-sample by the test harness). The
// only things removed are Max dependencies: buffer~ access goes through a host
// callback interface (karma_buffer_iface), and the list-outlet/clock UI plumbing
// is elided (it never touches audio or the buffer).
//
// The public surface intentionally mirrors the reference (`t_karma`,
// `karma_record`, `karma_mono_perform`, ...) so the same driver/scenarios can
// exercise it unchanged.

#ifndef KARMA_CORE_H
#define KARMA_CORE_H

#include <math.h>
#include <string.h>

// --- scalar types the copied code expects (Max-free equivalents) -----------
typedef long  t_ptr_int;
typedef char  t_bool;
typedef void  t_object;      // perform sig has an (unused) t_object* dsp64
typedef void  t_buffer_obj;  // opaque buffer handle (the host's, via the iface)
#ifndef true
#define true 1
#define false 0
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef CLAMP
#define CLAMP(a, lo, hi) ( (a)>(lo)?( (a)<(hi)?(a):(hi) ):(lo) )
#endif

// Interpolation kernels (verbatim from the reference)
#define LINEAR_INTERP(f, x, y) (x + f*(y - x))
#define CUBIC_INTERP(f, w, x, y, z) ((((0.5*(z - w) + 1.5*(x - y))*f + (w - 2.5*x + y + y - 0.5*z))*f + (0.5*(y - w)))*f + x)
#define SPLINE_INTERP(f, w, x, y, z) (((-0.5*w + 1.5*x - 1.5*y + 0.5*z)*f*f*f) + ((w - 2.5*x + y + y - 0.5*z)*f*f) + ((-0.5*w + 0.5*y)*f) + x)

// UI/clock plumbing is a no-op in the core (host owns reporting).
#define clock_delay(x, n) ((void)0)
#define clock_unset(x)    ((void)0)
#define object_error(...) ((void)0)
// Inlet routing is a host concern. Returning 0 matches the offline stub used by
// the reference oracle (so karma_float's "is this the speed inlet?" gate behaves
// identically); a real host maps the speed inlet to index == ochans.
#define proxy_getinlet(o) (0L)

// --- host buffer interface -------------------------------------------------
// The core never allocates or names the sample buffer; the host supplies it.
typedef struct {
    void  *(*lock)(void *ctx);   // returns float* to interleaved samples (or NULL)
    void   (*unlock)(void *ctx);
    void   (*set_dirty)(void *ctx);
    void   *ctx;
    long    frames;              // frames per channel
    long    chans;              // channels
    double  sr;                 // sample rate
} karma_buffer_iface;

// --- core state ------------------------------------------------------------
// Same fields as the reference t_karma, minus Max-object members
// (k_ob/buf/bufname/messout/tclock), plus the buffer interface.
typedef struct karma_core {
    karma_buffer_iface bufio;

    double  ssr, bsr, bmsr, srscale, vs, vsnorm, bvsnorm;
    double  o1prev, o2prev, o3prev, o4prev;
    double  o1dif, o2dif, o3dif, o4dif;
    double  writeval1, writeval2, writeval3, writeval4;
    double  playhead, maxhead, jumphead, selstart, selection;
    double  snrfade, overdubamp, overdubprev, speedfloat;

    long    syncoutlet, moduloout, islooped;

    t_ptr_int bframes, bchans, ochans, nchans;
    t_ptr_int interpflag, recordhead, minloop, maxloop, startloop, endloop;
    t_ptr_int pokesteps, recordfade, playfade, globalramp, snrramp, snrtype;
    t_ptr_int reportlist, initiallow, initialhigh;

    short   speedconnect;

    char    statecontrol, statehuman;
    char    playfadeflag, recfadeflag, recendmark, directionorig, directionprev;

    t_bool  stopallowed, go, record, recordprev, loopdetermine, alternateflag;
    t_bool  append, triginit, wrapflag, jumpflag;
    t_bool  recordinit, initinit, initskip, buf_modified, clockgo;
} t_karma;

// --- API (names mirror the reference) --------------------------------------
void karma_core_init(t_karma *x, long ochans, double ssr, double vs);
void karma_core_set_dims(t_karma *x);   // mirrors karma_buf_setup, from x->bufio

void karma_float(t_karma *x, double speedfloat);
void karma_stop(t_karma *x);
void karma_play(t_karma *x);
void karma_record(t_karma *x);
void karma_append(t_karma *x);
void karma_overdub(t_karma *x, double amplitude);
void karma_jump(t_karma *x, double jumpposition);
void karma_select_start(t_karma *x, double positionstart);
void karma_select_size(t_karma *x, double duration);

void karma_mono_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                        double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_stereo_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                          double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_quad_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                        double **outs, long nouts, long vcount, long flgs, void *usr);

#endif // KARMA_CORE_H
