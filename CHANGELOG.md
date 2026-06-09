# Changelog

All notable changes to this project are documented here. The format is loosely
based on [Keep a Changelog](https://keepachangelog.com/). The `karma_core` DSP is
held to a sample-exact differential against the reference `karma~`; entries below
preserved that bar (`cd tests && make check` green) unless noted.

## [Unreleased]

### karma_core refactor (Max-free DSP)

Refactored the extracted `karma_core` library from a verbatim copy of the
reference DSP into hand-owned, modular, readable source. `karma_core.c` went from
~7,700 to ~1,440 lines with behaviour proven identical to the reference across
all scenarios.

- **Unified the perform routines.** The reference's three near-identical
  mono/stereo/quad perform routines (each also branched internally on buffer
  channel count) are collapsed into a single channel-generic `karma_perform` that
  loops over `min(buffer, output)` channels and silences any output channel
  beyond the buffer's channel count. The three public `karma_*_perform` entry
  points are now thin forwarders, so the host shell is unaffected.
- **Named the state machine.** Added `karma_state.h`: value-for-value enums for
  `statecontrol`, `recfadeflag`, `playfadeflag`, `recendmark`, and `statehuman`,
  replacing the reference's undocumented magic ints (struct field widths, and the
  report-list integers the host forwards, are unchanged).
- **Modularised the kernels.** Split interpolation (`karma_interp.h`:
  LINEAR/CUBIC/SPLINE + `interp_index`) and the record/ipoke write engine
  (`karma_ipoke.h`: `ease_record` / `ease_switchramp` / `ease_bufoff` /
  `ease_bufon`) into reusable headers.
- **Removed dead state.** Dropped the unused `moduloout`, `islooped`,
  `reportlist`, and `clockgo` fields and the inert report-clock block from the
  core (the host shell owns the real report clock).
- **Normalised types.** Replaced the reference's pointer-sized `t_ptr_int` with
  the standard `int64_t` (64-bit on every Max target, including Windows LLP64
  where `long` would be 32-bit), removing a Max-ism from the Max-free core.
- **Retired the generator.** `gen_core.sh` (the scaffolding that produced the
  verified initial extraction) is kept as `gen_core.sh.orig`; `karma_core.c` is
  now hand-owned source.

### Test harness

- **Closed a coverage gap before unifying.** The harness previously allocated the
  sample buffer with the same channel count as the object's outputs, so the
  `buffer-channels < output-channels` paths were never diff-tested. Decoupled the
  two in the drivers and added six `pchans < ochans` scenarios (e.g.
  stereo-over-mono-buffer, quad-over-2/3-channel-buffer); 21 scenarios total now
  guard every channel-count path against the reference.
- **Added `make bench`.** Perform-only throughput of the unified core vs the
  reference's unrolled routines (`bench_core` / `bench_ref`). The unified loop is
  measurably slower per output sample on multichannel record paths but negligible
  in absolute real-time terms (sub-0.1% CPU per instance at 48 kHz); kept the
  single routine for maintainability.

### Fixed

- **`karma_re~`: `stop` / `jump` were silently gated off.** The shell's
  `karma_re_dsp64` never set the core's `initinit` flag (the reference sets it in
  its own `dsp64` on first DSP-on), and both harness drivers force-set it
  manually, masking the bug. `karma_re_dsp64` now sets `initinit`, and the shell
  test driver no longer overrides it, so the differential exercises the real init
  path. (Pre-existing shell bug; unrelated to the core refactor, which touched
  `karma_re~.c` by a single line.)

### Known limitations

- `karma_re~` calls `kre_buf_setup` on every `dsp64`, so toggling DSP off/on
  resets the loop selection to full instead of restoring it (the reference gates
  buffer setup on first-init and restores the stored selection otherwise). The
  single-`dsp64`-call harness does not cover this; not yet ported.

## Earlier work

- Extracted `karma_core` (Max-free) and re-hosted it as the thin `karma_re~`
  shell; built the offline differential harness, scenario catalogue, and kernel
  unit tests.
- Exploratory whole-external rewrites `k1~` / `k2~` / `k3~` (and the
  overdub-corruption characterisation of the `k4~` attempt). See `README.md`.
