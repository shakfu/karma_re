// Core driver: runs the scenario catalogue against the Max-free karma_core and
// writes build/core_<scenario>.bin, for sample-exact diffing against ref_*.bin.

#include <stdio.h>
#include <stdlib.h>

#include "max_stub.h"     // mock_buffer storage (shared with scenarios.h)
#include "karma_core.h"   // the Max-free core (defines t_karma, control + perform)
#include "scenarios.h"

// Host buffer interface backed by the same mock buffer scenarios.h reads.
static void *core_lock(void *ctx)     { return ((mock_buffer *)ctx)->data; }
static void  core_unlock(void *ctx)   { (void)ctx; }
static void  core_setdirty(void *ctx) { (void)ctx; }

static t_karma *construct(long frames, long chans, double sr)
{
    float *data = (float *)calloc((size_t)(frames * chans), sizeof(float));
    mock_buffer_install(data, frames, chans, sr);

    t_karma *x = (t_karma *)malloc(sizeof(t_karma));
    karma_core_init(x, chans, sr, SCN_VS);
    x->bufio.lock      = core_lock;
    x->bufio.unlock    = core_unlock;
    x->bufio.set_dirty = core_setdirty;
    x->bufio.ctx       = mock_buffer_get();
    x->bufio.frames    = frames;
    x->bufio.chans     = chans;
    x->bufio.sr        = sr;
    karma_core_set_dims(x);

    x->speedconnect = 0;
    x->speedfloat   = 1.0;
    return x;
}

static void perform(t_karma *x, double **ins, long nins, double **outs, long nouts, long vcount)
{
    switch (x->ochans) {
        case 1:  karma_mono_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
        case 2:  karma_stereo_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL); break;
        default: karma_quad_perform(x, NULL, ins, nins, outs, nouts, vcount, 0, NULL);   break;
    }
}

int main(void)
{
    printf("=== karma_core ===\n");
    run_all_scenarios(construct, perform, "core", 4);
    printf("OK\n");
    return 0;
}
