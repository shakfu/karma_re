// Candidate driver: runs the same scenario catalogue against k4~.c and writes
// build/k4_<scenario>.bin, for sample-exact diffing against the reference.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "max_stub.h"
#include "k4~.c"         // candidate implementation (defines its own t_karma)
#include "scenarios.h"

// Construct a fresh k4 karma~ bound to a zeroed mock buffer.
// bchans = buffer~ channel count; ochans = object (outlet) channel count.
static t_karma *construct(long frames, long bchans, long ochans, double sr)
{
    static int classed = 0;
    if (!classed) { ext_main(NULL); classed = 1; }

    float *bufdata = (float *)calloc((size_t)(frames * bchans), sizeof(float));
    mock_buffer_install(bufdata, frames, bchans, sr);

    t_atom argv[2];
    atom_setsym(&argv[0], gensym("mockbuf"));
    atom_setlong(&argv[1], ochans);
    t_karma *x = (t_karma *)karma_new(gensym("k4~"), 2, argv);
    if (!x) return NULL;

    x->timing.ssr    = sr;
    x->timing.vs     = SCN_VS;
    x->timing.vsnorm = x->timing.vs / x->timing.ssr;
    karma_buf_setup(x, gensym("mockbuf"));
    x->speedconnect    = 1;     // speed driven via the signal inlet (in_speed)
    x->speedfloat      = 1.0;
    x->state.initinit  = 1;     // DSP "on" -- enables jump/stop control methods
    return x;
}

static void perform(t_karma *x, double **ins, long nins, double **outs, long nouts, long vcount)
{
    switch (x->buffer.ochans) {
        case 1:  karma_mono_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
        case 2:  karma_stereo_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL); break;
        default: karma_poly_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
    }
}

int main(void)
{
    printf("=== k4 candidate ===\n");
    // poly (>2ch) is the MC routing path; with syncoutlet=0 it matches the harness I/O
    // contract (ins[0..n-1]=audio, ins[n]=speed, outs[0..n-1]=audio), so drive up to 4ch.
    run_all_scenarios(construct, perform, NULL, "k4", 4);
    printf("OK\n");
    return 0;
}
