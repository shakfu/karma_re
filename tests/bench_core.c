// Perform-only microbenchmark for the unified karma_core routine.
// Fills an initial loop, then times steady-state playback+overdub perform calls
// and reports ns per output sample for 1 / 2 / 4 output channels. Compare against
// bench_ref (the reference's unrolled routines) to judge the loop overhead.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "max_stub.h"
#include "karma_core.h"

#define BFRAMES 16384
#define VS      64
#define WARM    4096          // vectors to establish the loop
#define ITERS   200000        // timed perform calls

static void *bl(void *c){ return ((mock_buffer*)c)->data; }
static void  bu(void *c){ (void)c; }
static void  bd(void *c){ (void)c; }

static void perform(t_karma *x, double **ins, double **outs, long chans)
{
    switch (chans) {
        case 1:  karma_mono_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL);   break;
        case 2:  karma_stereo_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL); break;
        default: karma_quad_perform(x, NULL, ins, chans+1, outs, chans, VS, 0, NULL);   break;
    }
}

static t_karma *mk(long chans)
{
    float *data = (float*)calloc((size_t)(BFRAMES*chans), sizeof(float));
    mock_buffer_install(data, BFRAMES, chans, 48000.0);
    t_karma *x = (t_karma*)malloc(sizeof(t_karma));
    karma_core_init(x, chans, 48000.0, VS);
    x->bufio.lock=bl; x->bufio.unlock=bu; x->bufio.set_dirty=bd;
    x->bufio.ctx=mock_buffer_get(); x->bufio.frames=BFRAMES; x->bufio.chans=chans; x->bufio.sr=48000.0;
    karma_core_set_dims(x);
    x->speedconnect=1; x->speedfloat=1.0; x->initinit=1;
    return x;
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

    karma_record(x);                                   // record initial loop
    for (long v=0; v<WARM; v++) perform(x, ins, outs, chans);
    karma_play(x);                                     // steady-state playback
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
    printf("=== karma_core (unified) perform-only ===\n");
    for (long c=1;c<=4;c*=2)
        printf("  %ld-ch: %.3f ns/sample\n", c, bench(c));
    return 0;
}
