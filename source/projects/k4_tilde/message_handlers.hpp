#ifndef KARMA_MESSAGE_HANDLERS_HPP
#define KARMA_MESSAGE_HANDLERS_HPP

#include "state_machine.hpp"

// =============================================================================
// KARMA MESSAGE HANDLERS - User Message Processing
// =============================================================================
// Functions that handle Max messages from the user (stop, play, record, append,
// overdub, jump). These set control state flags that are processed during DSP.

namespace karma {

/**
 * @brief Handle stop message
 *
 * Stops playback/recording if allowed. Uses alternate stop mode if alternateflag
 * is set, otherwise uses regular stop mode.
 *
 * @param x Karma object (modified - updates statecontrol, append, statehuman, stopallowed)
 */
inline void handle_stop(t_karma* x) noexcept
{
    if (x->state.initinit) {
        if (x->state.stopallowed) {
            x->state.statecontrol = x->state.alternateflag ? control_state_t::STOP_ALT
                                                           : control_state_t::STOP_REGULAR;
            x->state.append = 0;
            x->state.statehuman = human_state_t::STOP;
            x->state.stopallowed = 0;
        }
    }
}

/**
 * @brief Handle play message
 *
 * Starts or resumes playback. Handles various state transitions:
 * - If appending while not playing: enters append mode
 * - If recording/appending: stops recording/appending
 * - Otherwise: starts playback
 *
 * @param x Karma object (modified - updates statecontrol, go, statehuman, stopallowed, snrfade)
 */
inline void handle_play(t_karma* x) noexcept
{
    if ((!x->state.go) && (x->state.append)) {
        x->state.statecontrol = control_state_t::APPEND;
        x->fade.snrfade = 0.0;
    } else if ((x->state.record) || (x->state.append)) {
        x->state.statecontrol = x->state.alternateflag ? control_state_t::PLAY_ALT
                                                       : control_state_t::RECORD_OFF;
    } else {
        x->state.statecontrol = control_state_t::PLAY_ON;
    }

    x->state.go = 1;
    x->state.statehuman = human_state_t::PLAY;
    x->state.stopallowed = 1;
}

/**
 * @brief Clear buffer contents
 *
 * Helper function to zero out buffer contents. Used when starting initial recording.
 *
 * @param buf Buffer object
 * @param bframes Frame count
 * @param rchans Channel count
 * @return true if successful, false if buffer lock failed
 */
inline t_bool clear_buffer(t_buffer_obj* buf, long bframes, long rchans) noexcept
{
    float* b = buffer_locksamples(buf);
    if (!b)
        return 0;
    for (long i = 0; i < bframes; i++) {
        for (long c = 0; c < rchans; c++) {
            b[i * rchans + c] = 0.0f;
        }
    }
    buffer_setdirty(buf);
    buffer_unlocksamples(buf);
    return 1;
}

/**
 * @brief Handle record message
 *
 * Complex state machine for recording transitions:
 * - If already recording: toggle overdub or stop recording
 * - If appending: start recording in append mode or initial loop
 * - If not playing: clear buffer and start initial loop
 * - Otherwise: start overdub
 *
 * @param x Karma object (modified - updates statecontrol, statehuman, go, recordinit, stopallowed)
 */
inline void handle_record(t_karma* x) noexcept
{
    t_buffer_obj*   buf = buffer_ref_getobject(x->buffer.buf);
    control_state_t sc = control_state_t::ZERO;
    human_state_t   sh = x->state.statehuman;
    t_bool          record = x->state.record;
    t_bool          go = x->state.go;
    t_bool          altflag = x->state.alternateflag;
    t_bool          append = x->state.append;
    t_bool          init = x->state.recordinit;

    x->state.stopallowed = 1;

    if (record) {
        if (altflag) {
            sc = control_state_t::RECORD_ALT;
            sh = human_state_t::OVERDUB;
        } else {
            sc = control_state_t::RECORD_OFF;
            sh = (sh == human_state_t::OVERDUB) ? human_state_t::PLAY : human_state_t::RECORD;
        }
    } else if (append) {
        if (go) {
            if (altflag) {
                sc = control_state_t::RECORD_ALT;
                sh = human_state_t::OVERDUB;
            } else {
                sc = control_state_t::APPEND_SPECIAL;
                sh = human_state_t::APPEND;
            }
        } else {
            sc = control_state_t::RECORD_INITIAL_LOOP;
            sh = human_state_t::INITIAL;
        }
    } else if (!go) {
        init = 1;
        if (buf) {
            long rchans = x->buffer.bchans;
            long bframes = x->buffer.bframes;
            clear_buffer(buf, bframes, rchans);
        }
        sc = control_state_t::RECORD_INITIAL_LOOP;
        sh = human_state_t::INITIAL;
    } else {
        sc = control_state_t::RECORD_ON;
        sh = human_state_t::OVERDUB;
    }

    x->state.go = 1;
    x->state.recordinit = init;
    x->state.statecontrol = sc;
    x->state.statehuman = sh;
}

/**
 * @brief Handle append message
 *
 * Enables append mode, allowing recording to extend beyond the current loop.
 * Only allowed after initial loop has been created.
 *
 * @param x Karma object (modified - updates append, maxloop, statecontrol, statehuman, stopallowed)
 */
inline void handle_append(t_karma* x) noexcept
{
    if (x->state.recordinit) {
        if ((!x->state.append) && (!x->state.loopdetermine)) {
            x->state.append = 1;
            x->loop.maxloop = (x->buffer.bframes - 1);
            x->state.statecontrol = control_state_t::APPEND;
            x->state.statehuman = human_state_t::APPEND;
            x->state.stopallowed = 1;
        } else {
            object_error(
                reinterpret_cast<t_object*>(x),
                "can't append if already appending, or during 'initial-loop', "
                "or if buffer~ is full");
        }
    } else {
        object_error(
            reinterpret_cast<t_object*>(x),
            "warning! no 'append' registered until at least one loop has been "
            "created first");
    }
}

/**
 * @brief Handle overdub message
 *
 * Sets overdub amplitude (mix level between existing and new audio).
 *
 * @param x Karma object (modified - updates overdubamp)
 * @param amplitude Overdub amplitude 0.0-1.0 (clamped)
 */
inline void handle_overdub(t_karma* x, double amplitude) noexcept
{
    x->audio.overdubamp = CLAMP(amplitude, 0.0, 1.0);
}

/**
 * @brief Handle jump message
 *
 * Jumps to a specific position within the loop with crossfade.
 * Position is phase-based (0.0 = loop start, 1.0 = loop end).
 *
 * @param x Karma object (modified - updates statecontrol, jumphead, stopallowed)
 * @param jumpposition Position in loop (0.0-1.0, clamped)
 */
inline void handle_jump(t_karma* x, double jumpposition) noexcept
{
    if (x->state.initinit) {
        if (!((x->state.loopdetermine) && (!x->state.record))) {
            x->state.statecontrol = control_state_t::JUMP;
            x->timing.jumphead = CLAMP(jumpposition, 0., 1.);
            x->state.stopallowed = 1;
        }
    }
}

} // namespace karma

#endif // KARMA_MESSAGE_HANDLERS_HPP
