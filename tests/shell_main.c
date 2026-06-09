// Shell driver: runs the scenario catalogue against the Max shell karma_re~.c
// (driven offline via the Max stub) and writes build/shell_<scenario>.bin, to
// confirm the re-hosted external reproduces the reference sample-for-sample.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "max_stub.h"
#include "karma_re~.c"        // the shell (defines t_karma_re, karma_re_*, ext_main)

#define SCN_DATA_ONLY
#include "scenarios.h"        // scenario catalogue (data only)

static t_karma_re *construct(long frames, long bchans, long ochans, double sr)
{
    static int classed = 0;
    if (!classed) { ext_main(NULL); classed = 1; }

    float *data = (float *)calloc((size_t)(frames * bchans), sizeof(float));
    mock_buffer_install(data, frames, bchans, sr);

    t_atom argv[2];
    atom_setsym(&argv[0], gensym("mockbuf"));
    atom_setlong(&argv[1], ochans);
    t_karma_re *x = (t_karma_re *)karma_re_new(gensym("karma_re~"), 2, argv);
    if (!x) return NULL;

    short count[8] = {1,1,1,1,1,1,1,1};   // all inlets "connected" -> speed is a signal
    karma_re_dsp64(x, NULL, count, sr, SCN_VS, 0);
    x->core.initinit   = 1;   // enable jump/stop
    x->core.speedfloat = 1.0;
    return x;
}

static void perform(t_karma_re *x, double **ins, long nins, double **outs, long nouts, long vcount)
{
    switch (x->core.ochans) {
        case 1:  karma_re_mono_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
        case 2:  karma_re_stereo_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL); break;
        default: karma_re_quad_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
    }
}

static void fire(t_karma_re *x, const sc_event *e)
{
    switch (e->op) {
        case OP_REC:      karma_re_record(x);          break;
        case OP_PLAY:     karma_re_play(x);            break;
        case OP_STOP:     karma_re_stop(x);            break;
        case OP_OVERDUB:  karma_re_overdub(x, e->arg); break;
        case OP_APPEND:   karma_re_append(x);          break;
        case OP_JUMP:     karma_re_jump(x, e->arg);    break;
        case OP_FLOAT:    /* speed is a signal here */ break;
        case OP_SELSTART: karma_re_position(x, e->arg);break;
        case OP_SELSIZE:  karma_re_window(x, e->arg);  break;
    }
}

static void run_one(t_karma_re *x, const scenario *sc, FILE *out)
{
    long chans = sc->chans;
    double in_audio[SCN_MAXCHANS][SCN_VS], in_speed[SCN_VS], out_audio[SCN_MAXCHANS][SCN_VS];
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
            else fire(x, e);
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

    // data/report outlet capture (matches scenarios.h's write_report format)
    double  rep[32];
    int64_t nr = 0;
    mock_outlet_reset();
    karma_re_clock_list(x);
    long rc = mock_outlet_count();
    if (rc > 0) { nr = (rc > 32) ? 32 : rc; for (long i = 0; i < nr; i++) rep[i] = mock_outlet_value(i); }
    fwrite(&nr, sizeof(nr), 1, out);
    fwrite(rep, sizeof(double), (size_t)nr, out);
}

int main(void)
{
    printf("=== karma_re~ shell ===\n");
    char path[512];
    for (int s = 0; s < N_SCENARIOS; s++) {
        const scenario *sc = &g_scenarios[s];
        t_karma_re *x = construct(sc->frames, scn_bchans(sc), sc->chans, sc->sr);
        if (!x) continue;
        snprintf(path, sizeof(path), "shell_%s.bin", sc->name);
        FILE *f = fopen(path, "wb");
        if (!f) continue;
        run_one(x, sc, f);
        fclose(f);
        printf("  wrote %s\n", path);
        free(mock_buffer_get()->data);
        free(x);
    }
    printf("OK\n");
    return 0;
}
