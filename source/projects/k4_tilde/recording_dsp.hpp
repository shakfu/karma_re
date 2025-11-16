#ifndef KARMA_RECORDING_DSP_HPP
#define KARMA_RECORDING_DSP_HPP

#include "dsp_utils.hpp"
#include "fade_engine.hpp"

// =============================================================================
// KARMA RECORDING DSP - Recording Audio Processing
// =============================================================================
// Functions for recording audio with iPoke interpolation, fade management,
// and jump logic.

namespace karma {

/**
 * @brief Process iPoke recording with interpolation
 *
 * Records audio with linear interpolation/averaging to handle variable speeds.
 * Uses iPoke technique: averaging for speed < 1x, interpolation for speed > 1x.
 *
 * @param b Buffer pointer
 * @param pchans Buffer channel count
 * @param playhead Current playback position
 * @param recordhead Record head position (modified)
 * @param recin1 Input sample to record
 * @param overdubamp Overdub amplitude (unused in current implementation)
 * @param globalramp Global ramp length
 * @param recordfade Recording fade counter
 * @param recfadeflag Recording fade flag
 * @param pokesteps iPoke steps counter (modified)
 * @param writeval1 Write value accumulator (modified)
 * @param dirt Buffer modified flag (set to 1)
 */
inline void process_ipoke_recording(
    float* b,
    long pchans,
    long playhead,
    long* recordhead,
    double recin1,
    double overdubamp,
    double globalramp,
    long recordfade,
    char recfadeflag,
    double* pokesteps,
    double* writeval1,
    t_bool* dirt) noexcept
{
    // Handle first record head initialization
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
        double recplaydif = static_cast<double>(playhead - *recordhead);
        if (recplaydif > 0) { // linear-interpolation for speed > 1x
            double coeff1 = (recin1 - *writeval1) / recplaydif;
            for (long i = *recordhead + 1; i < playhead; i++) {
                *writeval1 += coeff1;
                b[i * pchans] = static_cast<float>(*writeval1);
            }
        } else {
            double coeff1 = (recin1 - *writeval1) / recplaydif;
            for (long i = *recordhead - 1; i > playhead; i--) {
                *writeval1 -= coeff1;
                b[i * pchans] = static_cast<float>(*writeval1);
            }
        }
        *writeval1 = recin1;
    }
    *recordhead = playhead;
    *dirt = 1;
}

/**
 * @brief Process recording fade state machine
 *
 * Manages recording fade counter and transitions between recording states.
 * Handles fade completion and triggers state changes.
 *
 * @param globalramp Global ramp length in samples
 * @param recordfade Recording fade counter (modified)
 * @param recfadeflag Recording fade flag (modified)
 * @param record Recording active flag (modified)
 * @param triginit Trigger initialization flag (modified)
 * @param jumpflag Jump mode flag (modified)
 */
inline void process_recording_fade(
    double globalramp,
    long* recordfade,
    char* recfadeflag,
    t_bool* record,
    t_bool* triginit,
    t_bool* jumpflag) noexcept
{
    if (globalramp) { // realtime ramps for record on/off
        if (*recordfade < globalramp) {
            (*recordfade)++;
            if ((*recfadeflag) && (*recordfade >= globalramp)) {
                if (*recfadeflag == 2) {
                    *triginit = *jumpflag = 1;
                    *recordfade = 0;
                } else if (*recfadeflag == 5) {
                    *record = 1;
                } else {
                    *record = 0;
                }
                *recfadeflag = 0;
            }
        }
    } else {
        if (*recfadeflag) {
            if (*recfadeflag == 2) {
                *triginit = *jumpflag = 1;
            } else if (*recfadeflag == 5) {
                *record = 1;
            } else {
                *record = 0;
            }
            *recfadeflag = 0;
        }
    }
}

/**
 * @brief Process jump logic and positioning
 *
 * Calculates jump position based on jumphead parameter and original
 * recording direction. Applies appropriate fades.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified)
 * @param jumpflag Jump flag (cleared after processing)
 * @param direction Current playback direction
 */
inline void process_jump_logic(
    t_karma* x,
    float* b,
    double* accuratehead,
    t_bool* jumpflag,
    char direction) noexcept
{
    if (*jumpflag) { // jump
        if (x->state.directionorig >= 0) {
            *accuratehead = x->timing.jumphead * x->timing.maxhead;
        } else {
            *accuratehead = (x->buffer.bframes - 1)
                - (((x->buffer.bframes - 1) - x->timing.maxhead) * x->timing.jumphead);
        }
        *jumpflag = 0;
        x->fade.snrfade = 0.0;
        if (x->state.record) {
            if (x->fade.globalramp) {
                ease_buffer_fadein(
                    x->buffer.bframes - 1, b, x->buffer.nchans,
                    static_cast<long>(*accuratehead),
                    x->timing.recordhead, direction, x->fade.globalramp);
                x->fade.recordfade = 0;
            }
            x->fade.recfadeflag = 0;
            x->timing.recordhead = -1;
        }
        x->state.triginit = 0;
    }
}

} // namespace karma

#endif // KARMA_RECORDING_DSP_HPP
