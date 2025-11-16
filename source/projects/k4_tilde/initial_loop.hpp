#ifndef KARMA_INITIAL_LOOP_HPP
#define KARMA_INITIAL_LOOP_HPP

#include "dsp_utils.hpp"
#include "fade_engine.hpp"

// =============================================================================
// KARMA INITIAL LOOP - Initial Loop Recording and Boundary Management
// =============================================================================
// Functions for managing the initial loop creation phase, including iPoke
// recording with direction reversal support and buffer boundary constraints.

namespace karma {

/**
 * @brief Process iPoke recording during initial loop creation
 *
 * Handles iPoke interpolation recording during the initial loop creation phase.
 * This is more complex than regular recording because it must handle direction
 * reversals that can occur while creating the first loop. When direction changes,
 * the function determines the shortest interpolation path considering wrap-around.
 *
 * The function implements:
 * - Averaging for speeds < 1x (pokesteps > 1)
 * - Linear interpolation for speeds > 1x
 * - Wrap-around logic when direction != directionorig
 * - Shortest-path calculation for direction reversals
 *
 * @param b Buffer pointer
 * @param pchans Buffer channel count (interleaved stride)
 * @param recordhead Record head position (modified)
 * @param playhead Current playback position
 * @param recin1 Input sample to record
 * @param pokesteps iPoke steps accumulator (modified)
 * @param writeval1 Write value accumulator (modified)
 * @param direction Current playback direction
 * @param directionorig Original recording direction
 * @param maxhead Maximum head position reached during recording
 * @param frames Total buffer frame count
 */
inline void process_initial_loop_ipoke_recording(
    float* b,
    long pchans,
    long* recordhead,
    long playhead,
    double recin1,
    double* pokesteps,
    double* writeval1,
    char direction,
    char directionorig,
    long maxhead,
    long frames) noexcept
{
    long   i;
    double recplaydif, coeff1;

    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = static_cast<float>(*writeval1);
        recplaydif = static_cast<double>(playhead - *recordhead); // linear-interp for speed > 1x

        if (direction != directionorig) {
            if (directionorig >= 0) {
                if (recplaydif > 0) {
                    if (recplaydif > (maxhead * 0.5)) {
                        recplaydif -= maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i >= 0; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                        apply_ipoke_interpolation(
                            b, pchans, maxhead, playhead, writeval1, coeff1, -1);
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    }
                } else {
                    if ((-recplaydif) > (maxhead * 0.5)) {
                        recplaydif += maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < (maxhead + 1); i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                        for (i = 0; i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    }
                }
            } else {
                if (recplaydif > 0) {
                    if (recplaydif > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif -= ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i >= maxhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                        for (i = (frames - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    }
                } else {
                    if ((-recplaydif) > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif += ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead + 1); i < frames; i++) {
                            *writeval1 += coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                        apply_ipoke_interpolation(
                            b, pchans, maxhead, playhead, writeval1, coeff1, 1);
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            b[i * pchans] = static_cast<float>(*writeval1);
                        }
                    }
                }
            }
        } else {
            if (recplaydif > 0) {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                for (i = (*recordhead + 1); i < playhead; i++) {
                    *writeval1 += coeff1;
                    b[i * pchans] = static_cast<float>(*writeval1);
                }
            } else {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                for (i = (*recordhead - 1); i > playhead; i--) {
                    *writeval1 -= coeff1;
                    b[i * pchans] = static_cast<float>(*writeval1);
                }
            }
        }
        *writeval1 = recin1;
    }
}

/**
 * @brief Process boundary constraints during initial loop creation
 *
 * Handles playhead advancement and buffer boundary constraints during the
 * initial loop creation phase. Implements:
 * - Speed limiting during recording (prevents overly fast recording)
 * - Buffer wraparound detection (reaching start or end)
 * - Maximum position tracking (maxhead)
 * - Loop completion detection and state transitions
 * - Direction reversal boundary handling
 *
 * When playhead reaches buffer boundaries, this function:
 * - Applies fadeout at the boundary
 * - Marks loop as complete (recendmark, triginit)
 * - Transitions to playback or append mode
 * - Resets recording state appropriately
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified - advanced and wrapped)
 * @param speed Current playback speed
 * @param direction Current playback direction
 */
inline void process_initial_loop_boundary_constraints(
    t_karma* x,
    float* b,
    double* accuratehead,
    double speed,
    char direction) noexcept
{
    long   setloopsize;
    double speedsrscaled;

    setloopsize = x->loop.maxloop
        - x->loop.minloop; // not really required here because initial loop ??
    speedsrscaled = speed * x->timing.srscale;
    if (x->state.record) // speed limiting during record
        speedsrscaled = (fabs(speedsrscaled) > (setloopsize / KARMA_SPEED_LIMIT_DIVISOR))
            ? ((setloopsize / KARMA_SPEED_LIMIT_DIVISOR) * direction)
            : speedsrscaled;
    *accuratehead = *accuratehead + speedsrscaled;

    if (direction
        == x->state.directionorig) { // buffer~ boundary constraints and registry of
                                     // maximum distance traversed
        if (*accuratehead > (x->buffer.bframes - 1)) {
            *accuratehead = 0.0;
            x->state.record = x->state.append;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    ease_buffer_fadeout(
                        x->buffer.bframes - 1, b, x->buffer.nchans,
                        (x->buffer.bframes - 1), -direction,
                        x->fade.globalramp); // maxloop ??
                    x->timing.recordhead = -1;
                    x->fade.recfadeflag = x->fade.recordfade = 0;
                }
            }
            x->state.recendmark = x->state.triginit = 1;
            x->state.loopdetermine = x->state.alternateflag = 0;
            x->timing.maxhead = x->buffer.bframes - 1;
        } else if (*accuratehead < 0.0) {
            *accuratehead = x->buffer.bframes - 1;
            x->state.record = x->state.append;
            if (x->state.record) {
                if (x->fade.globalramp) {
                    ease_buffer_fadeout(
                        x->buffer.bframes - 1, b, x->buffer.nchans, x->loop.minloop,
                        -direction,
                        x->fade.globalramp); // 0.0  // ??
                    x->timing.recordhead = -1;
                    x->fade.recfadeflag = x->fade.recordfade = 0;
                }
            }
            x->state.recendmark = x->state.triginit = 1;
            x->state.loopdetermine = x->state.alternateflag = 0;
            x->timing.maxhead = 0.0;
        } else { // <- track max write position
            if (((x->state.directionorig >= 0) && (x->timing.maxhead < *accuratehead))
                || ((x->state.directionorig < 0)
                    && (x->timing.maxhead > *accuratehead))) {
                x->timing.maxhead = *accuratehead;
            }
        }
    } else if (direction < 0) { // wraparounds for reversal while creating initial-loop
        if (*accuratehead < 0.0) {
            *accuratehead = x->timing.maxhead + *accuratehead;
            if (x->fade.globalramp) {
                ease_buffer_fadeout(
                    x->buffer.bframes - 1, b, x->buffer.nchans, x->loop.minloop,
                    -direction, x->fade.globalramp); // 0.0  // ??
                x->timing.recordhead = -1;
                x->fade.recfadeflag = x->fade.recordfade = 0;
            }
        }
    } else if (direction >= 0) {
        if (*accuratehead > (x->buffer.bframes - 1)) {
            *accuratehead = x->timing.maxhead + (*accuratehead - (x->buffer.bframes - 1));
            if (x->fade.globalramp) {
                ease_buffer_fadeout(
                    x->buffer.bframes - 1, b, x->buffer.nchans, (x->buffer.bframes - 1),
                    -direction,
                    x->fade.globalramp); // maxloop ??
                x->timing.recordhead = -1;
                x->fade.recfadeflag = x->fade.recordfade = 0;
            }
        }
    }
}

} // namespace karma

#endif // KARMA_INITIAL_LOOP_HPP
