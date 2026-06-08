// Reference driver: runs the scenario catalogue against the unmodified
// reference karma~.c and writes golden output to build/ref_<scenario>.bin

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "max_stub.h"
#include "karma~.c"      // reference implementation (defines t_karma)
#include "scenarios.h"   // impl-agnostic runner (uses the symbols above)

// Construct a fresh reference karma~ bound to a zeroed mock buffer.
static t_karma *construct(long frames, long chans, double sr)
{
    static int classed = 0;
    if (!classed) { ext_main(NULL); classed = 1; }

    float *bufdata = (float *)calloc((size_t)(frames * chans), sizeof(float));
    mock_buffer_install(bufdata, frames, chans, sr);

    t_atom argv[2];
    atom_setsym(&argv[0], gensym("mockbuf"));
    atom_setlong(&argv[1], chans);
    t_karma *x = (t_karma *)karma_new(gensym("karma~"), 2, argv);
    if (!x) return NULL;

    x->ssr    = sr;
    x->vs     = SCN_VS;
    x->vsnorm = x->vs / x->ssr;
    karma_buf_setup(x, gensym("mockbuf"));
    x->speedconnect = 0;
    x->speedfloat   = 1.0;
    return x;
}

int main(void)
{
    printf("=== reference oracle ===\n");
    run_all_scenarios(construct, "ref");
    printf("OK\n");
    return 0;
}
