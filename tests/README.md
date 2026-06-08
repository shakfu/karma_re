# karma offline test harness

Goal: extract karma's DSP into a portable, Max-free core with a real test suite,
then re-host it in Max as a thin shell. This directory drives the DSP **outside
of Max** so behaviour is deterministic, inspectable, and diff-able.

## What works today

`make` builds two offline drivers, runs every scenario through each, and diffs
their output + final buffer **sample-for-sample**:

- `max_stub.c` / `max_stub.h` implement just enough of the Max runtime (buffer~,
  atoms, class/object lifecycle, outlets, clock, attributes, sysmem) over a
  single malloc'd buffer. Types come from the real c74 headers; only the called
  functions are stubbed (~50).
- `scenarios.h` is the impl-agnostic runner: a catalogue of scripted gestures
  (input signal + timed control events) plus `run_scenario` / `run_all_scenarios`.
  It touches only the shared public API, so the same code drives any impl.
- `oracle_main.c` drives the **reference** `karma~.c` -> `build/ref_*.bin`
  (the behavioural "oracle" / golden data).
- `k4_main.c` drives the candidate `k4~.c` -> `build/k4_*.bin`.
- `diff.c` reports first divergence + magnitude per scenario.

### Current finding

k4's recording diverges from the reference from buffer frame 1 even without
overdub (`rec_play`: 508/16384 frames differ, max|d|=0.18), and is near-total
during `overdub_boundary` (16382/16384 frames differ, max|d|=0.56) -- an
objective confirmation of the overdub corruption.

## karma_core (Max-free)

`karma_core.{h,c}` is the DSP extracted out of Max. `karma_core.c` is **generated**
from the reference by `gen_core.sh`: it copies the DSP helpers, control methods,
and mono perform verbatim, then rewrites the Max dependencies -- buffer~ access
becomes a host callback interface (`karma_buffer_iface`), and the list-outlet/
clock UI plumbing is elided. Field names and float math are unchanged, so it is
behaviourally identical to the reference.

`make` builds the core driver and confirms it: **the mono path currently matches
the reference sample-for-sample (output + buffer) across every scenario.**

To re-extract after touching the reference: `./gen_core.sh && make`.

## Roadmap

1. [done] Drive the reference DSP offline (feasibility spike).
2. [done] Scenario library + golden capture + reference/k4 differential.
3. [in progress] Extract `karma_core` (Max-free), diffed to zero against the
   reference. **Mono done.** Remaining: stereo + quad perform.
4. Harness: drive variable speed/jump directly (currently `reverse`/`speed_half`
   run as forward playback since karma_float's inlet gate is off offline), add
   stereo + append + jump scenarios.
5. Unit tests for the pure pieces (interpolators, ipoke, wrap/boundary math).
6. Re-host in Max as a thin shell over `karma_core`.

## Build

```
make            # build both drivers, run all scenarios, diff ref vs k4
make oracle     # just run the reference driver
make k4         # just run the candidate driver
make clean
```

Per-scenario manual diff (with a tolerance to ignore denormal noise):

```
build/difftool --tol 1e-9 build/ref_overdub_boundary.bin build/k4_overdub_boundary.bin
```

Requires the c74 SDK at `../source/max-sdk-base` (already present).
