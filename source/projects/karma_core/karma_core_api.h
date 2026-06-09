// karma_core public API.
//
// This header declares the core state struct, the host buffer interface, and the
// control + perform functions. Integer fields use the standard int64_t (pulled in
// below); it deliberately does NOT define the remaining Max scalar types it uses
// (t_bool / t_object) or any shim macros, so it can be included either:
//   - by a Max host, after the real c74 headers (which provide those types), or
//   - by the standalone core build, after karma_core.h (which provides them).
//
// int64_t (not the reference's pointer-sized int64_t, nor `long`) is used for
// the buffer-index/sample-count fields: it is 64-bit on every platform, so the
// struct layout is identical to the reference's int64_t on all Max targets
// (incl. Windows LLP64, where `long` would be only 32-bit).
//
// Field names and float math mirror the reference karma~ exactly, so the core is
// behaviourally identical (verified sample-for-sample by the offline harness).

#ifndef KARMA_CORE_API_H
#define KARMA_CORE_API_H

#include <stdint.h>     // int64_t

// --- host buffer interface -------------------------------------------------
// The core never allocates or names the sample buffer; the host supplies it
// (a Max buffer~, a malloc'd array, etc.) through these callbacks.
typedef struct {
    void  *(*lock)(void *ctx);    // -> float* interleaved samples (or NULL)
    void   (*unlock)(void *ctx);
    void   (*set_dirty)(void *ctx);
    void   *ctx;
    long    frames;               // frames per channel
    long    chans;                // channels
    double  sr;                   // sample rate
} karma_buffer_iface;

// --- core state ------------------------------------------------------------
// Same fields as the reference t_karma, minus its Max-object members
// (k_ob / buf / bufname / messout / tclock), plus the buffer interface.
typedef struct karma_core {
    karma_buffer_iface bufio;

    double  ssr, bsr, bmsr, srscale, vs, vsnorm, bvsnorm;
    double  o1prev, o2prev, o3prev, o4prev;
    double  o1dif, o2dif, o3dif, o4dif;
    double  writeval1, writeval2, writeval3, writeval4;
    double  playhead, maxhead, jumphead, selstart, selection;
    double  snrfade, overdubamp, overdubprev, speedfloat;

    long    syncoutlet;

    int64_t bframes, bchans, ochans, nchans;
    int64_t interpflag, recordhead, minloop, maxloop, startloop, endloop;
    int64_t pokesteps, recordfade, playfade, globalramp, snrramp, snrtype;
    int64_t initiallow, initialhigh;

    short   speedconnect;

    char    statecontrol, statehuman;
    char    playfadeflag, recfadeflag, recendmark, directionorig, directionprev;

    t_bool  stopallowed, go, record, recordprev, loopdetermine, alternateflag;
    t_bool  append, triginit, wrapflag, jumpflag;
    t_bool  recordinit, initinit, initskip, buf_modified;
} t_karma;

// --- lifecycle / configuration ---------------------------------------------
void karma_core_init(t_karma *x, long ochans, double ssr, double vs);
void karma_core_set_dims(t_karma *x);   // mirrors karma_buf_setup, reads x->bufio
// Set loop start/end. points_flag: 0 = phase, 1 = samples, 2 = ms; low/high < 0
// mean "unset" -> defaults (0 / full buffer). (resetloop = call with the stored
// initiallow/initialhigh in samples.)
void karma_core_set_loop(t_karma *x, double low, double high, long points_flag);

// --- control (names mirror the reference messages) -------------------------
void karma_float(t_karma *x, double speedfloat);
void karma_stop(t_karma *x);
void karma_play(t_karma *x);
void karma_record(t_karma *x);
void karma_append(t_karma *x);
void karma_overdub(t_karma *x, double amplitude);
void karma_jump(t_karma *x, double jumpposition);
void karma_select_start(t_karma *x, double positionstart);
void karma_select_size(t_karma *x, double duration);

// --- per-vector DSP ---------------------------------------------------------
void karma_mono_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                        double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_stereo_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                          double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_quad_perform(t_karma *x, t_object *dsp64, double **ins, long nins,
                        double **outs, long nouts, long vcount, long flgs, void *usr);

#endif // KARMA_CORE_API_H
