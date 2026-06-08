# TODO: refactor karma_core (next session)

## Where things stand

`source/projects/karma_core/` is the Max-free DSP, **extracted but not refactored**.
`karma_core.c` is generated verbatim from the reference `karma~.c` by
`gen_core.sh` (helpers + control methods + the three perform routines), with only
the Max dependencies rewritten (buffer~ -> `karma_buffer_iface`; clock/outlet/
error -> shims). The internal DSP is the reference's code unchanged: three
massive near-duplicate perform routines, giant inline bodies, cryptic flag soup.

The Max external `source/projects/karma_re_tilde/karma_re~.c` is a thin shell
over the core.

## The point: refactoring is now SAFE and verifiable

`cd tests && make check` proves, sample-for-sample:
- `karma_core` == reference `karma~` (15 scenarios x mono/stereo/quad, output + buffer)
- `karma_re~` shell == reference (audio + the 7-element data/report outlet)
- kernel unit tests (interp / ease / ipoke / wrap / set_loop)

So any refactor that keeps `make check` green is provably behaviour-preserving,
and the moment a change diverges the harness names the exact sample. This is the
safety net that was missing when the k4 attempt went sideways.

## Step 0 (do this first): retire the generator

`gen_core.sh` was scaffolding to reach a *verified* starting point. Hand-editing
the generated `karma_core.c` would be clobbered on the next regenerate. So:

1. Stop regenerating. Treat `karma_core.c` as hand-owned source from now on.
2. Move/keep `gen_core.sh` as historical reference (e.g. `gen_core.sh.orig`), or
   delete it; drop the "GENERATED -- do not edit" banner from `karma_core.c`.
3. From here, refactor `karma_core.c` directly, `make check` after every step.

## Refactor targets (each gated by `make check` == green)

1. **Modularize.** Split the monolith into translation units the original author
   themselves wanted (`karma~.c` header note: "take perform routines out, put
   interpolation and ipoke into separate files"):
   - `karma_interp.h` (LINEAR/CUBIC/SPLINE + `interp_index`) -- mostly done as macros.
   - `karma_ipoke.{h,c}` (the record/ipoke write engine + ease fades).
   - `karma_engine.c` (the perform state machine).
2. **Unify the three perform routines.** mono/stereo/quad are ~1k/2k/4k lines of
   the same logic unrolled per channel. Collapse to one channel-generic routine
   (this is exactly what k4 tried to do, but now with the differential as a net).
   Watch performance: keep the hot path tight; consider a templated/loop form for
   N channels with the 1/2 cases still fast.
3. **Tame the flag soup.** `statecontrol` / `statehuman` / `recendmark` /
   `alternateflag` / `recfadeflag` / `playfadeflag` are an undocumented state
   machine. Map it out (a state diagram), name the states, and replace magic
   ints (recfadeflag == 2/5, recendmark == 0..4, playfadeflag == 0..4) with enums.
4. **Drop dead/unused fields.** `moduloout`, `islooped`, `reportlist` (core),
   the inert `clockgo` block in the perform tail (the shell owns the real clock),
   leftover `t_object* dsp64` perform params, etc.
5. **Normalise types.** The reference's `t_ptr_int` everywhere is noise in a
   Max-free core; use plain `long`/`int64_t`. Beware the struct ABI shared with
   the shell (see `karma_core_api.h` -- keep field sizes identical or update both).

## Then: features the original never built (optional, separate)

`multiply` / `offset` / `loop` (on/off) / `modout` are commented-out TODOs in
`karma~` -- net-new features, no reference to diff against. If wanted, implement
post-refactor so defaults stay reference-identical (existing differential still
guards no-regression) and add unit tests for the feature-active paths.

## Guardrails / gotchas

- After ANY core change: `cd tests && make check` must stay green.
- The shell embeds `t_karma` by value -> changing the struct layout means the
  shell sees a different ABI. `karma_core_api.h` is shared by both; keep it in sync.
- The report clock: the shell owns it via its own `clockgo`; the core's verbatim
  clock block is inert (no-op shims). When modularizing, just delete the core's
  clock block rather than leaving the dead toggle.
- `tests/README.md` documents the harness; `source/projects/karma_core/README.md`
  documents the library

## Checkout karma2 ideas:

see: <https://github.com/rconstanzo/karma2>

.
