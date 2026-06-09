// karma_state.h -- named states for karma's control/perform state machine.
//
// The reference encodes its state machine as bare magic ints. These enums put
// names to those values WITHOUT changing them: every enumerator equals the
// literal the reference used, so the struct fields stay their original width
// (char) and the behaviour (and the report-list integers the host forwards) are
// byte-identical. They exist purely to make the flag soup legible.
//
// How the machine runs (raja's "all-in-one switch"):
//   1. A control method (karma_record/play/stop/jump/append) writes a single
//      `statecontrol` (SC_*) value -- the pending message.
//   2. The next perform vector's switch consumes that SC_* once, sets up the
//      corresponding record/play flags and the fade flags below, then clears
//      statecontrol back to SC_ZERO.
//   3. The fade flags (recfadeflag / playfadeflag) and the loop-end mark
//      (recendmark) then drive the per-sample fades inside the perform loop.
//   `statehuman` (SH_*) is the user-facing state, reported out, never consumed.

#ifndef KARMA_STATE_H
#define KARMA_STATE_H

// statecontrol -- the pending control message (set by a control method, consumed
// once by the next perform vector). Comments preserve raja's case annotations.
typedef enum {
    SC_ZERO           = 0,   // no pending message
    SC_REC_INITIAL    = 1,   // record initial loop
    SC_REC_ALT        = 2,   // record alternateflag -- into overdub
    SC_REC_OFF        = 3,   // record off, regular
    SC_PLAY_ALT       = 4,   // play alternateflag -- out of overdub
    SC_PLAY_ON        = 5,   // play on, regular
    SC_STOP_ALT       = 6,   // stop alternateflag -- after overdub
    SC_STOP           = 7,   // stop, regular
    SC_JUMP           = 8,   // jump
    SC_APPEND         = 9,   // append
    SC_APPEND_SPECIAL = 10,  // special append -- into record/overdub
    SC_REC_ON         = 11   // record on, regular
} karma_statecontrol;

// recfadeflag -- pending record-fade action (value 3 is unused by the reference).
typedef enum {
    REC_FADE_NONE = 0,   // no record fade pending
    REC_FADE_OUT  = 1,   // fade out: stop recording
    REC_FADE_JUMP = 2,   // fade then jump (arms triginit + jumpflag)
    REC_FADE_IN   = 5    // fade in: resume recording
} karma_recfadeflag;

// playfadeflag -- pending play-fade action.
typedef enum {
    PLAY_FADE_NONE   = 0,   // no play fade pending
    PLAY_FADE_OUT    = 1,   // fade out: stop playing
    PLAY_FADE_JUMP   = 2,   // fade then jump
    PLAY_FADE_RECOFF = 3,   // fade for record-off: continue playing
    PLAY_FADE_APPEND = 4    // fade for append
} karma_playfadeflag;

// recendmark -- why the loop end is being marked (gates loop-end recomputation).
typedef enum {
    RECEND_NONE          = 0,   // consumed / disabled
    RECEND_GENERIC       = 1,   // generic end-of-loop (stop alternateflag)
    RECEND_EXIT_OVERDUB  = 2,   // record-off / exit overdub (play alternateflag)
    RECEND_ENTER_OVERDUB = 3,   // record-on / enter overdub (record alternateflag)
    RECEND_CROSSING      = 4    // loop boundary crossed during playback
} karma_recendmark;

// statehuman -- user-facing state, forwarded in the report list (never consumed).
typedef enum {
    SH_STOP         = 0,
    SH_PLAY         = 1,
    SH_REC_EXISTING = 2,   // recording onto an existing loop
    SH_OVERDUB      = 3,
    SH_APPEND       = 4,
    SH_REC_INITIAL  = 5    // recording the initial loop
} karma_statehuman;

// Coverage note: the control methods, the central `switch (statecontrol)`
// dispatch, the `switch (playfadeflag)` blocks, and the recfadeflag/playfadeflag
// equality tests all use these names. The four small `switch (recendmark)`
// fall-through tables inside the initial-loop path still use bare 0..4 case
// labels (their values map directly onto RECEND_* above) -- they are a compact,
// localised state table cross-referenced here, left numeric to avoid obscuring
// the deliberate case fall-through.

#endif // KARMA_STATE_H
