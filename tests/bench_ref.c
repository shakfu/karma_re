// Perform-only microbenchmark for the REFERENCE karma~ (unrolled mono/stereo/quad
// routines), the baseline for bench_core. Same workload + timing methodology.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "max_stub.h"
#include "karma~.c"

#define BFRAMES 16384
#define VS      64
#define WARM    4096
#define ITERS   200000

static t_karma *mk(long chans)
{
    static int classed = 0;
    if (!classed) { ext_main(NULL); classed = 1; }
    float *data = (float*)calloc((size_t)(BFRAMES*chans), sizeof(float));
    mock_buffer_install(data, BFRAMES, chans, 48000.0);
    t_atom argv[2];
    atom_setsym(&argv[0], gensym("mockbuf"));
    atom_setlong(&argv[1], chans);
    t_karma *x = (t_karma*)karma_new(gensym("karma~"), 2, argv);
    x->ssr=48000.0; x->vs=VS; x->vsnorm=x->vs/x->ssr;
    karma_buf_setup(x, gensym("mockbuf"));
    x->speedconnect=1; x->speedfloat=1.0; x->initinit=1;
    return x;
}

static void perform(t_karma *x, double **ins, double **outs, long chans)
{
    switch (chans) {
        case 1:  karma_mono_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL);   break;
        case 2:  karma_stereo_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL); break;
        default: karma_quad_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL);   break;
    }
}

static double bench(long chans)
{
    t_karma *x = mk(chans);
    double in_a[4][VS], in_s[VS], out_a[4][VS];
    double *ins[5], *outs[5];
    for (long c=0;c<chans;c++){ ins[c]=in_a[c]; outs[c]=out_a[c]; }
    ins[chans]=in_s;
    double ph=0;
    for (int i=0;i<VS;i++){ in_s[i]=1.0; for(long c=0;c<chans;c++){ in_a[c][i]=0.25*sin(ph); } ph+=0.01; }

    karma_record(x);
    for (long v=0; v<WARM; v++) perform(x, ins, outs, chans);
    karma_play(x);
    karma_overdub(x, 0.5);
    for (long v=0; v<WARM; v++) perform(x, ins, outs, chans);

    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long v=0; v<ITERS; v++) perform(x, ins, outs, chans);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (t1.tv_sec-t0.tv_sec)*1e9 + (t1.tv_nsec-t0.tv_nsec);
    double samples = (double)ITERS * VS * chans;
    free(mock_buffer_get()->data); free(x);
    return ns / samples;
}

int main(void)
{
    printf("=== reference karma~ (unrolled) perform-only ===\n");
    for (long c=1;c<=4;c*=2)
        printf("  %ld-ch: %.3f ns/sample\n", c, bench(c));
    return 0;
}
