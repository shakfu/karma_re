// Scenario catalogue + runner for the karma~ DSP harness.
//
// DATA (scenario definitions) is always available. The RUNNER (scn_fire /
// run_scenario / run_all_scenarios) calls the shared public API by name
// (karma_record / karma_mono_perform / ...) and is compiled only when the
// includer does NOT define SCN_DATA_ONLY. A driver whose object type/method
// names differ (e.g. the Max shell, which wraps the core) defines SCN_DATA_ONLY
// and supplies its own runner over the same catalogue.

#ifndef KARMA_SCENARIOS_H
#define KARMA_SCENARIOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SCN_VS       64   // vector size used for all scenarios
#define SCN_MAXCHANS 4

// ----- control events ------------------------------------------------------
enum {
    OP_REC, OP_PLAY, OP_STOP, OP_OVERDUB, OP_APPEND,
    OP_JUMP, OP_FLOAT, OP_SELSTART, OP_SELSIZE
};

typedef struct { long at; int op; double arg; } sc_event;

typedef struct {
    const char    *name;
    long           frames;   // buffer frames per channel
    long           chans;    // buffer/object channels (1 / 2 / 4)
    double         sr;
    double         in_freq;  // base input sine freq (Hz); channel c uses freq*(c+1)
    double         in_amp;
    long           total;    // total samples to run (multiple of SCN_VS)
    const sc_event *events;
    int            nevents;
} scenario;

// ----- catalogue -----------------------------------------------------------
// Loop length = time recorded before OP_PLAY, so overdub passes cross the loop
// boundary repeatedly.

static const sc_event ev_rec_play[] = {
    { 0,    OP_REC,  0 },
    { 8192, OP_PLAY, 0 },
};
static const sc_event ev_overdub_boundary[] = {
    { 0,     OP_REC,     0   },
    { 8192,  OP_PLAY,    0   },
    { 12288, OP_OVERDUB, 1.0 },
    { 12288, OP_REC,     0   },
    { 40000, OP_PLAY,    0   },
};
static const sc_event ev_reverse[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 }, { 12288, OP_FLOAT, -1.0 },
};
static const sc_event ev_speed_half[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 }, { 12288, OP_FLOAT, 0.5 },
};
static const sc_event ev_speed_double[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 }, { 12288, OP_FLOAT, 2.0 },
};
static const sc_event ev_speed_sweep[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 },
    { 12288, OP_FLOAT, 1.5 }, { 20480, OP_FLOAT, -1.0 }, { 28672, OP_FLOAT, 0.25 },
};
static const sc_event ev_jump[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 }, { 12288, OP_JUMP, 0.5 }, { 18432, OP_JUMP, 0.0 },
};
static const sc_event ev_stop[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 }, { 16384, OP_STOP, 0 },
};
static const sc_event ev_window[] = {
    { 0, OP_REC, 0 }, { 8192, OP_PLAY, 0 },
    { 12288, OP_SELSTART, 0.25 }, { 12288, OP_SELSIZE, 0.5 },
};

static const scenario g_scenarios[] = {
    { "rec_play",             16384, 1, 48000.0, 220.0, 0.25, 24576, ev_rec_play,         2 },
    { "rec_play_st",          16384, 2, 48000.0, 220.0, 0.25, 24576, ev_rec_play,         2 },
    { "rec_play_quad",        16384, 4, 48000.0, 220.0, 0.25, 24576, ev_rec_play,         2 },
    { "overdub_boundary",     16384, 1, 48000.0, 220.0, 0.25, 49152, ev_overdub_boundary, 5 },
    { "overdub_boundary_st",  16384, 2, 48000.0, 220.0, 0.25, 49152, ev_overdub_boundary, 5 },
    { "overdub_boundary_quad",16384, 4, 48000.0, 220.0, 0.25, 49152, ev_overdub_boundary, 5 },
    { "reverse",              16384, 1, 48000.0, 220.0, 0.25, 32768, ev_reverse,          3 },
    { "reverse_st",           16384, 2, 48000.0, 220.0, 0.25, 32768, ev_reverse,          3 },
    { "speed_half",           16384, 1, 48000.0, 220.0, 0.25, 32768, ev_speed_half,       3 },
    { "speed_double",         16384, 1, 48000.0, 220.0, 0.25, 32768, ev_speed_double,     3 },
    { "speed_sweep",          16384, 1, 48000.0, 220.0, 0.25, 40960, ev_speed_sweep,      5 },
    { "speed_sweep_quad",     16384, 4, 48000.0, 220.0, 0.25, 40960, ev_speed_sweep,      5 },
    { "jump",                 16384, 1, 48000.0, 220.0, 0.25, 24576, ev_jump,             4 },
    { "stop",                 16384, 1, 48000.0, 220.0, 0.25, 24576, ev_stop,             3 },
    { "window",               16384, 1, 48000.0, 220.0, 0.25, 32768, ev_window,           4 },
};
#define N_SCENARIOS ((int)(sizeof(g_scenarios)/sizeof(g_scenarios[0])))

#ifndef SCN_DATA_ONLY
// ===========================================================================
// Runner: drives an object whose control/perform API is the shared `karma_*`
// names (reference, k4, core). The Max shell defines SCN_DATA_ONLY instead.
// ===========================================================================
#include "max_stub.h"

typedef void (*perform_fn)(t_karma *x, double **ins, long nins,
                           double **outs, long nouts, long vcount);
typedef t_karma *(*construct_fn)(long frames, long chans, double sr);

static void scn_fire(t_karma *x, const sc_event *e)
{
    switch (e->op) {
        case OP_REC:      karma_record(x);               break;
        case OP_PLAY:     karma_play(x);                 break;
        case OP_STOP:     karma_stop(x);                 break;
        case OP_OVERDUB:  karma_overdub(x, e->arg);      break;
        case OP_APPEND:   karma_append(x);               break;
        case OP_JUMP:     karma_jump(x, e->arg);         break;
        case OP_FLOAT:    /* speed is a signal here */    break;
        case OP_SELSTART: karma_select_start(x, e->arg); break;
        case OP_SELSIZE:  karma_select_size(x, e->arg);  break;
    }
}

// Run one scenario; write captured output (interleaved) + final buffer to `out`:
//   [int64 n_out][double out[n_out]][int64 n_buf][float buf[n_buf]]
static void run_scenario(t_karma *x, const scenario *sc, perform_fn perform, FILE *out)
{
    long chans = sc->chans;
    double in_audio[SCN_MAXCHANS][SCN_VS], in_speed[SCN_VS];
    double out_audio[SCN_MAXCHANS][SCN_VS];
    double *ins[SCN_MAXCHANS + 1], *outs[SCN_MAXCHANS + 1];
    for (long c = 0; c < chans; c++) { ins[c] = in_audio[c]; outs[c] = out_audio[c]; }
    ins[chans] = in_speed;
    long nins = chans + 1, nouts = chans;

    long    n_out = sc->total * chans;
    double *cap   = (double *)calloc((size_t)n_out, sizeof(double));
    double phase[SCN_MAXCHANS] = {0};
    const double dphase = (sc->in_freq > 0) ? (2.0 * M_PI * sc->in_freq / sc->sr) : 0.0;
    double cur_speed = 1.0;

    int ei = 0;
    for (long base = 0; base < sc->total; base += SCN_VS) {
        while (ei < sc->nevents && sc->events[ei].at <= base) {
            const sc_event *e = &sc->events[ei++];
            if (e->op == OP_FLOAT) cur_speed = e->arg;
            else scn_fire(x, e);
        }
        for (int i = 0; i < SCN_VS; i++) {
            for (long c = 0; c < chans; c++) {
                in_audio[c][i] = (sc->in_freq > 0) ? (sc->in_amp * sin(phase[c])) : 0.0;
                phase[c] += dphase * (double)(c + 1);
                out_audio[c][i] = 0.0;
            }
            in_speed[i] = cur_speed;
        }
        perform(x, ins, nins, outs, nouts, SCN_VS);
        for (int i = 0; i < SCN_VS; i++)
            for (long c = 0; c < chans; c++)
                cap[(base + i) * chans + c] = out_audio[c][i];
    }

    mock_buffer *mb = mock_buffer_get();
    int64_t no = n_out, nb = (int64_t)(mb->frames * mb->chans);
    fwrite(&no, sizeof(no), 1, out);
    fwrite(cap, sizeof(double), (size_t)no, out);
    fwrite(&nb, sizeof(nb), 1, out);
    fwrite(mb->data, sizeof(float), (size_t)nb, out);
    free(cap);
}

static void run_all_scenarios(construct_fn make, perform_fn perform,
                              const char *impl, long max_chans)
{
    char path[512];
    for (int s = 0; s < N_SCENARIOS; s++) {
        const scenario *sc = &g_scenarios[s];
        if (sc->chans > max_chans) { printf("  skip %s (%ld ch)\n", sc->name, sc->chans); continue; }
        t_karma *x = make(sc->frames, sc->chans, sc->sr);
        if (!x) { fprintf(stderr, "construct failed for %s\n", sc->name); continue; }
        snprintf(path, sizeof(path), "%s_%s.bin", impl, sc->name);
        FILE *f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "cannot open %s\n", path); continue; }
        run_scenario(x, sc, perform, f);
        fclose(f);
        printf("  wrote %s\n", path);
        mock_buffer *mb = mock_buffer_get();
        free(mb->data);
        free(x);
    }
}
#endif // SCN_DATA_ONLY

#endif // KARMA_SCENARIOS_H
