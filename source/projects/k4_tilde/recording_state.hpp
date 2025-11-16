#ifndef KARMA_RECORDING_STATE_HPP
#define KARMA_RECORDING_STATE_HPP

#include "fade_engine.hpp"

// =============================================================================
// KARMA RECORDING STATE - Recording and Loop State Management
// =============================================================================
// Functions for managing recording state transitions, fade completion,
// and loop initialization after recording.

namespace karma {

/**
 * @brief Process recording fade completion state machine
 *
 * Handles state transitions when recording fades complete. Updates
 * loop bounds and triggers appropriate flags based on recfadeflag value.
 *
 * @param recfadeflag Recording fade flag (0-5)
 * @param recendmark Recording end marker (modified based on flag)
 * @param record Recording active flag
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 * @param loopdetermine Loop determination flag
 * @param recordfade Recording fade counter
 * @param directionorig Original recording direction
 * @param maxloop Maximum loop point (may be updated)
 * @param maxhead Maximum playhead position reached
 * @param frames Total buffer frames
 */
inline void process_recording_fade_completion(
    char recfadeflag,
    char* recendmark,
    t_bool* record,
    t_bool* triginit,
    t_bool* jumpflag,
    t_bool* loopdetermine,
    long* recordfade,
    char directionorig,
    long* maxloop,
    long maxhead,
    long frames) noexcept
{
    if (recfadeflag == 2) {
        *recendmark = 4;
        *triginit = *jumpflag = 1;
        *recordfade = 0;
    } else if (recfadeflag == 5) {
        *record = 1;
    }

    switch (*recendmark) {
        case 0:
            *record = 0;
            break;
        case 1:
            if (directionorig < 0) {
                *maxloop = (frames - 1) - maxhead;
            } else {
                *maxloop = maxhead;
            }
            // Fall through
        case 2:
            *record = *loopdetermine = 0;
            *triginit = 1;
            break;
        case 3:
            *record = *triginit = 1;
            *recordfade = *loopdetermine = 0;
            break;
        case 4:
            *recendmark = 0;
            break;
    }
}

/**
 * @brief Process playback fade state machine
 *
 * Handles playback fade state transitions. Controls go/stop behavior,
 * jump triggers, and append mode activation.
 *
 * @param playfadeflag Playback fade flag (0-4)
 * @param go Playback active flag
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 * @param loopdetermine Loop determination flag
 * @param playfade Playback fade counter
 * @param snrfade Switch and ramp fade value
 * @param record Recording active flag (read-only)
 */
inline void process_playfade_state(
    char* playfadeflag,
    t_bool* go,
    t_bool* triginit,
    t_bool* jumpflag,
    t_bool* loopdetermine,
    long* playfade,
    double* snrfade,
    t_bool record) noexcept
{
    switch (*playfadeflag) {
        case 0:
            break;
        case 1:
            *playfadeflag = *go = 0;
            break;
        case 2:
            if (!record)
                *triginit = *jumpflag = 1;
            // Fall through to case 3
        case 3:
            *playfadeflag = *playfade = 0;
            break;
        case 4:  // append
            *go = *triginit = *loopdetermine = 1;
            *snrfade = 0.0;
            *playfade = 0;
            *playfadeflag = 0;
            break;
    }
}

/**
 * @brief Handle loop initialization after recording
 *
 * Calculates and sets loop boundaries after initial recording completes.
 * Handles both forward and reverse recording directions, applies fades,
 * and sets up window/selection parameters.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified)
 * @param direction Current playback direction
 * @param setloopsize Loop size (modified)
 * @param wrapflag Wrap mode flag (modified)
 * @param recendmark_ptr Recording end marker (modified)
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 */
inline void process_loop_initialization(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction,
    long* setloopsize,
    t_bool* wrapflag,
    char* recendmark_ptr,
    t_bool triginit,
    t_bool jumpflag) noexcept
{
    if (triginit) {
        if (x->state.recendmark) {  // calculate end of loop
            if (x->state.directionorig >= 0) {
                x->loop.maxloop = CLAMP(x->timing.maxhead, KARMA_MIN_LOOP_SIZE, x->buffer.bframes - 1);
                *setloopsize = x->loop.maxloop - x->loop.minloop;
                *accuratehead = x->loop.startloop = x->loop.minloop + (x->timing.selstart * (*setloopsize));
                x->loop.endloop = x->loop.startloop + (x->timing.selection * (*setloopsize));
                if (x->loop.endloop > x->loop.maxloop) {
                    x->loop.endloop = x->loop.endloop - ((*setloopsize) + 1);
                    *wrapflag = 1;
                } else {
                    *wrapflag = 0;
                }
                if (direction < 0) {
                    if (x->fade.globalramp)
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                }
            } else {
                x->loop.maxloop = CLAMP((x->buffer.bframes - 1) - x->timing.maxhead, KARMA_MIN_LOOP_SIZE, x->buffer.bframes - 1);
                *setloopsize = x->loop.maxloop - x->loop.minloop;
                x->loop.startloop = ((x->buffer.bframes - 1) - (*setloopsize)) + (x->timing.selstart * (*setloopsize));
                if (x->loop.endloop > (x->buffer.bframes - 1)) {
                    x->loop.endloop = ((x->buffer.bframes - 1) - (*setloopsize)) + (x->loop.endloop - x->buffer.bframes);
                    *wrapflag = 1;
                } else {
                    *wrapflag = 0;
                }
                *accuratehead = x->loop.endloop;
                if (direction > 0) {
                    if (x->fade.globalramp)
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                }
            }
            if (x->fade.globalramp)
                ease_buffer_fadeout(x->buffer.bframes - 1, b, x->buffer.nchans,
                                   static_cast<long>(x->timing.maxhead), -direction, x->fade.globalramp);
            x->fade.snrfade = 0.0;
            x->state.append = x->state.alternateflag = 0;
            *recendmark_ptr = 0;
        } else {    // jump / play (inside 'window')
            *setloopsize = x->loop.maxloop - x->loop.minloop;
            if (jumpflag)
                *accuratehead = (x->state.directionorig >= 0) ? ((x->timing.jumphead * (*setloopsize)) + x->loop.minloop) : (((x->buffer.bframes - 1) - (x->loop.maxloop)) + (x->timing.jumphead * (*setloopsize)));
            else
                *accuratehead = (direction < 0) ? x->loop.endloop : x->loop.startloop;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                      static_cast<long>(*accuratehead), x->timing.recordhead,
                                      direction, x->fade.globalramp);
                }
            }
            x->fade.snrfade = 0.0;
        }
    }
}

/**
 * @brief Handle initial loop creation state
 *
 * Processes state when creating the very first loop. Handles append mode,
 * regular start, and applies appropriate fades.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified)
 * @param direction Current playback direction
 * @param triginit_ptr Trigger initialization flag (modified)
 */
inline void process_initial_loop_creation(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction,
    t_bool* triginit_ptr) noexcept
{
    if (x->state.go) {
        if (x->state.triginit) {
            if (x->state.jumpflag) {
                // Jump logic handled elsewhere
            } else if (x->state.append) {
                x->fade.snrfade = 0.0;
                *triginit_ptr = 0;
                if (x->state.record) {
                    *accuratehead = x->timing.maxhead;
                    if (x->fade.globalramp) {
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                        x->fade.recordfade = 0;
                    }
                    x->state.alternateflag = 1;
                    x->fade.recfadeflag = 0;
                    x->timing.recordhead = -1;
                } else {
                    *accuratehead = (x->state.directionorig >= 0) ? 0.0 : (x->buffer.bframes - 1);
                    if (x->fade.globalramp) {
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                    }
                }
            } else {  // regular start
                x->fade.snrfade = 0.0;
                *triginit_ptr = 0;
                *accuratehead = (x->state.directionorig >= 0) ? 0.0 : (x->buffer.bframes - 1);
                if (x->state.record) {
                    if (x->fade.globalramp) {
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                        x->fade.recordfade = 0;
                    }
                    x->fade.recfadeflag = 0;
                    x->timing.recordhead = -1;
                } else {
                    if (x->fade.globalramp) {
                        ease_buffer_fadein(x->buffer.bframes - 1, b, x->buffer.nchans,
                                          static_cast<long>(*accuratehead), x->timing.recordhead,
                                          direction, x->fade.globalramp);
                    }
                }
            }
        }
    }
}

} // namespace karma

#endif // KARMA_RECORDING_STATE_HPP
