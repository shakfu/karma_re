#ifndef KARMA_PERFORM_UTILS_HPP
#define KARMA_PERFORM_UTILS_HPP

#include "fade_engine.hpp"

// =============================================================================
// KARMA PERFORM UTILITIES - Performance Loop Helpers
// =============================================================================
// Utility functions for managing the audio perform loop including variable
// initialization, direction changes, and record toggles.

namespace karma {

/**
 * @brief Initialize perform loop variables from karma object
 *
 * Extracts essential variables from the karma object at the start of
 * each perform cycle.
 *
 * @param x Karma object
 * @param accuratehead Playhead position (output)
 * @param playhead Integer playhead position (output)
 * @param wrapflag Wrap mode flag (output)
 */
inline void initialize_perform_vars(
    t_karma* x,
    double* accuratehead,
    long* playhead,
    t_bool* wrapflag) noexcept
{
    // Most variables now accessed directly from struct, only essential ones passed out
    *accuratehead = x->timing.playhead;
    *playhead = static_cast<long>(trunc(*accuratehead));
    *wrapflag = x->state.wrapflag;
}

/**
 * @brief Handle playback direction changes
 *
 * Detects direction changes and applies appropriate fades to avoid clicks.
 * Resets recording state when direction changes during recording.
 *
 * Note: Caller must set recordhead = -1 for record mode after calling this.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param directionprev Previous direction
 * @param direction Current direction
 */
inline void process_direction_change(
    t_karma* x,
    float* b,
    char directionprev,
    char direction) noexcept
{
    if (directionprev != direction) {
        if (x->state.record && x->fade.globalramp) {
            ease_buffer_fadeout(
                x->buffer.bframes - 1, b, x->buffer.nchans,
                static_cast<long>(x->timing.recordhead),
                -direction, x->fade.globalramp);
            x->fade.recordfade = x->fade.recfadeflag = 0;
            // recordhead = -1; // Note: this should be handled by caller
        }
        x->fade.snrfade = 0.0;
    }
}

/**
 * @brief Handle record enable/disable transitions
 *
 * Applies fades when toggling recording on/off to prevent clicks.
 * Manages recordhead and fade state appropriately.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Current playhead position
 * @param direction Current playback direction
 * @param speed Current playback speed
 * @param dirt Buffer modified flag (set to 1 when recording stops)
 */
inline void process_record_toggle(
    t_karma* x,
    float* b,
    double accuratehead,
    char direction,
    double speed,
    t_bool* dirt) noexcept
{
    if ((x->state.record - x->state.recordprev) < 0) { // samp @record-off
        if (x->fade.globalramp)
            ease_buffer_fadeout(
                x->buffer.bframes - 1, b, x->buffer.nchans,
                static_cast<long>(x->timing.recordhead),
                direction, x->fade.globalramp);
        x->timing.recordhead = -1;
        *dirt = 1;
    } else if ((x->state.record - x->state.recordprev) > 0) { // samp @record-on
        x->fade.recordfade = x->fade.recfadeflag = 0;
        if (speed < 1.0)
            x->fade.snrfade = 0.0;
        if (x->fade.globalramp)
            ease_buffer_fadeout(
                x->buffer.bframes - 1, b, x->buffer.nchans,
                static_cast<long>(accuratehead), -direction,
                x->fade.globalramp);
    }
}

} // namespace karma

#endif // KARMA_PERFORM_UTILS_HPP
