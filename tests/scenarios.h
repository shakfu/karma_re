// Impl-agnostic scenario runner for karma~ DSP.
//
// Include this AFTER the implementation (#include "karma~.c" / "k4~.c") so that
// `t_karma` and the perform/control symbols resolve to that implementation.
// Everything here touches only the shared public API (karma_record/play/stop/
// overdub/append/jump/float/select_* + karma_mono_perform) and the mock buffer,
// never implementation-specific struct fields -- so the exact same code drives
// both the reference and any candidate.

#ifndef KARMA_SCENARIOS_H
#define KARMA_SCENARIOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "max_stub.h"

#define SCN_VS 64   // vector size used for all scenarios

// ----- control events ------------------------------------------------------

enum {
    OP_REC, OP_PLAY, OP_STOP, OP_OVERDUB, OP_APPEND,
    OP_JUMP, OP_FLOAT, OP_SELSTART, OP_SELSIZE
};

typedef struct { long at; int op; double arg; } sc_event;

typedef struct {
    const char    *name;
    long           frames;   // buffer frames
    long           chans;    // buffer channels (1 = mono)
    double         sr;
    double         in_freq;  // input sine freq (Hz); <= 0 => silent input
    double         in_amp;
    long           total;    // total samples to run (multiple of SCN_VS)
    const sc_event *events;
    int            nevents;
} scenario;

// Each scenario is fresh: the driver supplies a constructor that allocates +
// installs a zeroed buffer and returns a ready-to-perform object.
typedef t_karma *(*construct_fn)(long frames, long chans, double sr);

// ----- one scenario --------------------------------------------------------

static void scn_fire(t_karma *x, const sc_event *e)
{
    switch (e->op) {
        case OP_REC:      karma_record(x);             break;
        case OP_PLAY:     karma_play(x);               break;
        case OP_STOP:     karma_stop(x);               break;
        case OP_OVERDUB:  karma_overdub(x, e->arg);    break;
        case OP_APPEND:   karma_append(x);             break;
        case OP_JUMP:     karma_jump(x, e->arg);       break;
        case OP_FLOAT:    karma_float(x, e->arg);      break;
        case OP_SELSTART: karma_select_start(x, e->arg); break;
        case OP_SELSIZE:  karma_select_size(x, e->arg);  break;
    }
}

// Run one scenario on object x; write captured output + final buffer to `out`
// as: [int64 n_out][double out[n_out]][int64 n_buf][float buf[n_buf]]
static void run_scenario(t_karma *x, const scenario *sc, FILE *out)
{
    double in_audio[SCN_VS], in_speed[SCN_VS], out_audio[SCN_VS], out_sync[SCN_VS];
    double *ins[2]  = { in_audio, in_speed };
    double *outs[2] = { out_audio, out_sync };

    long   n_out = sc->total;
    double *cap  = (double *)calloc((size_t)n_out, sizeof(double));

    double phase = 0.0;
    const double dphase = (sc->in_freq > 0) ? (2.0 * M_PI * sc->in_freq / sc->sr) : 0.0;

    int ei = 0;
    for (long base = 0; base < sc->total; base += SCN_VS) {
        // fire any control events due at/before this vector boundary
        while (ei < sc->nevents && sc->events[ei].at <= base)
            scn_fire(x, &sc->events[ei++]);

        for (int i = 0; i < SCN_VS; i++) {
            in_audio[i] = (sc->in_freq > 0) ? (sc->in_amp * sin(phase)) : 0.0;
            phase += dphase;
            in_speed[i]  = 1.0;
            out_audio[i] = out_sync[i] = 0.0;
        }

        karma_mono_perform(x, NULL, ins, 2, outs, 2, SCN_VS, 0, NULL);

        for (int i = 0; i < SCN_VS; i++)
            cap[base + i] = out_audio[i];
    }

    mock_buffer *mb = mock_buffer_get();
    int64_t no = n_out, nb = (int64_t)(mb->frames * mb->chans);
    fwrite(&no, sizeof(no), 1, out);
    fwrite(cap, sizeof(double), (size_t)no, out);
    fwrite(&nb, sizeof(nb), 1, out);
    fwrite(mb->data, sizeof(float), (size_t)nb, out);

    free(cap);
}

// ----- scenario catalogue --------------------------------------------------
//
// Buffers are small so loops wrap quickly. Loop length = how long we record
// before OP_PLAY, so overdub passes cross the loop boundary repeatedly.

static const sc_event ev_rec_play[] = {
    { 0,    OP_REC,  0 },
    { 8192, OP_PLAY, 0 },
};

static const sc_event ev_overdub_boundary[] = {
    { 0,     OP_REC,     0   },   // record initial loop
    { 8192,  OP_PLAY,    0   },   // close loop (length ~8192)
    { 12288, OP_OVERDUB, 1.0 },   // full-feedback overdub
    { 12288, OP_REC,     0   },   // engage overdub (record onto playing loop)
    { 40000, OP_PLAY,    0   },   // stop overdub, keep playing
};

static const sc_event ev_reverse[] = {
    { 0,     OP_REC,   0    },
    { 8192,  OP_PLAY,  0    },
    { 12288, OP_FLOAT, -1.0 },     // reverse playback
};

static const sc_event ev_speed_half[] = {
    { 0,     OP_REC,   0   },
    { 8192,  OP_PLAY,  0   },
    { 12288, OP_FLOAT, 0.5 },
};

static const scenario g_scenarios[] = {
    { "rec_play",         16384, 1, 48000.0, 220.0, 0.25, 24576, ev_rec_play,        2 },
    { "overdub_boundary", 16384, 1, 48000.0, 220.0, 0.25, 49152, ev_overdub_boundary,5 },
    { "reverse",          16384, 1, 48000.0, 220.0, 0.25, 32768, ev_reverse,         3 },
    { "speed_half",       16384, 1, 48000.0, 220.0, 0.25, 32768, ev_speed_half,      3 },
};
#define N_SCENARIOS ((int)(sizeof(g_scenarios)/sizeof(g_scenarios[0])))

// ----- run all -------------------------------------------------------------

static void run_all_scenarios(construct_fn make, const char *impl)
{
    char path[512];
    for (int s = 0; s < N_SCENARIOS; s++) {
        const scenario *sc = &g_scenarios[s];
        t_karma *x = make(sc->frames, sc->chans, sc->sr);
        if (!x) { fprintf(stderr, "construct failed for %s\n", sc->name); continue; }

        snprintf(path, sizeof(path), "%s_%s.bin", impl, sc->name);
        FILE *f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "cannot open %s\n", path); continue; }
        run_scenario(x, sc, f);
        fclose(f);
        printf("  wrote %s\n", path);

        mock_buffer *mb = mock_buffer_get();
        free(mb->data);
        free(x);   // leaks any impl sub-allocations; fine for a short test run
    }
}

#endif // KARMA_SCENARIOS_H
