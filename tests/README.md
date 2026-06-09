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

The extracted DSP library lives in `../source/projects/karma_core/`
(`karma_core.{h,c}`). `karma_core.c` is **hand-owned source**: it was originally
extracted verbatim from the reference (the retired `gen_core.sh.orig` copied the
DSP helpers, control methods, and the mono/stereo/quad perform routines, then
rewrote the Max dependencies -- buffer~ access becomes a host callback interface
(`karma_buffer_iface`), and the list-outlet/clock UI plumbing is elided). It is
now refactored directly; the harness below is what keeps it honest.

`make` builds the core driver against that library and confirms it: **mono,
stereo, and quad all match the reference sample-for-sample (output + buffer)
across every scenario**, including the multichannel overdub-boundary cases.

The drivers also capture the **data/report outlet** at the end of each scenario
(via the stub's `outlet_list` capture). `make shelldiff` diffs the shell's
`karma_re_clock_list` output against the reference's `karma_clock_list` (the
7-element position/state/loop-bounds list). This guards the report-list value
math; it does not exercise the report *clock arming* (a Max scheduling concern
with no offline equivalent).

After any change to `karma_core.c`, re-run `make` (or `make check`) here and keep
it green -- that differential is the safety net for the refactor.

## Roadmap

1. [done] Drive the reference DSP offline (feasibility spike).
2. [done] Scenario library + golden capture + reference/k4 differential.
3. [done] Extract `karma_core` (Max-free), diffed to zero against the reference.
   **Mono + stereo + quad all match.**
4. [done] Harden the harness: speed is now driven through the speed *signal*
   inlet (real variable speed + direction flips), `initinit` is enabled so
   jump/stop work, and the catalogue covers record/play/overdub-at-boundary,
   reverse, speed half/double/sweep, jump, stop, and a selection window across
   mono/stereo/quad (15 scenarios). All match the reference sample-for-sample.
5. [done] Kernel unit tests (`unit_kernels.c`): interpolation macros, ease/fade
   functions, ipoke buffer-fade, and interp-index wrap math, checked against
   hand-computed values (`make unit`).
6. [done] Re-host in Max as a thin shell over `karma_core`
   (`../source/projects/karma_re_tilde/karma_re~.c`). The shell owns only the
   Max plumbing (object/inlets/outlets/buffer~ ref/clock/attrs) and forwards
   buffer access, control messages, and perform to the core. Driven offline via
   the stub (`shell_main.c`), it matches the reference sample-for-sample on all
   15 scenarios (`make shelldiff`).

## Build

```
make            # full check: ref==core, ref==shell, and kernel unit tests
make diff       # ref-vs-core sample-exact differential
make shelldiff  # ref-vs-(karma_re~ shell) sample-exact differential
make unit       # kernel unit tests
make k4diff     # characterise how far k4 diverges from the reference
make oracle / make core / make shell / make k4   # run one driver
make clean
```

Per-scenario manual diff (with a tolerance to ignore denormal noise):

```
build/difftool --tol 1e-9 build/ref_overdub_boundary.bin build/k4_overdub_boundary.bin
```

Requires the c74 SDK at `../source/max-sdk-base` (already present).
