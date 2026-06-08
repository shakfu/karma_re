# karma_core

The karma looper DSP, extracted out of Max so it can be reused in other
projects (CLI tools, other plugin formats, etc.). No Max/MSP headers — the host
supplies the sample buffer through a small callback interface
(`karma_buffer_iface`); the Max external becomes a thin shell over this core.

## Files

- `karma_core.h` — public API: the state struct (`t_karma`), control methods
  (`karma_record` / `karma_play` / `karma_overdub` / ...), and the per-vector
  `karma_{mono,stereo,quad}_perform` routines, plus `karma_core_init` /
  `karma_core_set_dims`.
- `karma_core.c` — **generated** (do not hand-edit).
- `gen_core.sh` — generator. Extracts the DSP helpers, control methods, and the
  mono/stereo/quad perform routines verbatim from the reference
  `../karma_tilde/karma~.c`, then rewrites only the Max dependencies:
  buffer~ access → the host interface; list-outlet/clock UI plumbing → elided.

## Regenerate

```
./gen_core.sh
```

## Correctness

`karma_core.c` is held to a sample-exact bar: the offline harness in
`../../../tests/` runs the reference `karma~.c` and this core through an
identical battery of scenarios and diffs their output + final buffer. They
currently match exactly for mono, stereo, and quad. After editing the reference
(or the generator), re-run `gen_core.sh` and `make` in `tests/`.

## Headers

- `karma_core_api.h` — the public API (struct, buffer interface, function
  declarations). Include this **after** the host's own scalar types
  (`t_ptr_int` / `t_bool` / `t_object`): the real c74 headers in a Max host, or
  `karma_core.h` in the standalone build.
- `karma_core.h` — the standalone build config: Max-free scalar typedefs, no-op
  UI/clock shims, interpolation macros, then `#include "karma_core_api.h"`. Only
  `karma_core.c` includes this.

## Using it in a host

```c
karma_core_init(x, ochans, sample_rate, vector_size);
x->bufio = (karma_buffer_iface){ .lock=..., .unlock=..., .set_dirty=...,
                                 .ctx=..., .frames=..., .chans=..., .sr=... };
karma_core_set_dims(x);
// per control message: karma_record(x) / karma_overdub(x, amp) / ...
// per audio vector:    karma_mono_perform(x, NULL, ins, nins, outs, nouts, n, 0, NULL);
```

The Max external `../karma_re_tilde/karma_re~.c` is the reference host: a thin
shell that wraps a `karma_core` and backs the buffer interface with a Max
`buffer~`. It is validated against the original `karma~` sample-for-sample by the
offline harness in `../../../tests/` (`make shelldiff`).
