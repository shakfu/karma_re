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
static t_karma *construct(long frames, long chans, double sr)
{
    static int classed = 0;
    if (!classed) { ext_main(NULL); classed = 1; }

    float *bufdata = (float *)calloc((size_t)(frames * chans), sizeof(float));
    mock_buffer_install(bufdata, frames, chans, sr);

    t_atom argv[2];
    atom_setsym(&argv[0], gensym("mockbuf"));
    atom_setlong(&argv[1], chans);
    t_karma *x = (t_karma *)karma_new(gensym("k4~"), 2, argv);
    if (!x) return NULL;

    x->timing.ssr    = sr;
    x->timing.vs     = SCN_VS;
    x->timing.vsnorm = x->timing.vs / x->timing.ssr;
    karma_buf_setup(x, gensym("mockbuf"));
    x->speedconnect = 0;
    x->speedfloat   = 1.0;
    return x;
}

int main(void)
{
    printf("=== k4 candidate ===\n");
    run_all_scenarios(construct, "k4");
    printf("OK\n");
    return 0;
}
