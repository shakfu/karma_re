#ifndef KARMA_PLAYBACK_DSP_HPP
#define KARMA_PLAYBACK_DSP_HPP

#include "interpolation.hpp"
#include "fade_engine.hpp"
#include "recording_state.hpp"

// =============================================================================
// KARMA PLAYBACK DSP - Playback Interpolation and Ramp Processing
// =============================================================================
// Functions for audio playback with interpolation and smooth ramping.
// Supports mono, stereo, and multichannel (poly) configurations.

namespace karma {

/**
 * @brief Perform playback interpolation with appropriate algorithm
 *
 * Chooses interpolation type based on record flag and interp setting.
 * During recording, always uses linear interpolation for efficiency.
 *
 * @param frac Interpolation fraction (0.0 to 1.0)
 * @param b Buffer pointer
 * @param interp0 Index 0 for interpolation
 * @param interp1 Index 1 for interpolation
 * @param interp2 Index 2 for interpolation
 * @param interp3 Index 3 for interpolation
 * @param pchans Buffer channel count (interleaved stride)
 * @param interp Interpolation type (LINEAR/CUBIC/SPLINE)
 * @param record Recording active flag (forces linear if true)
 * @return Interpolated sample value
 */
inline double perform_playback_interpolation(
    double frac,
    float* b,
    long interp0,
    long interp1,
    long interp2,
    long interp3,
    long pchans,
    interp_type_t interp,
    t_bool record) noexcept
{
    if (record) {
        // If recording, use linear interpolation
        return linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
    } else {
        // Otherwise use specified interpolation type
        if (interp == interp_type_t::CUBIC) {
            return cubic_interp(frac, b[interp0 * pchans], b[interp1 * pchans],
                               b[interp2 * pchans], b[interp3 * pchans]);
        } else if (interp == interp_type_t::SPLINE) {
            return spline_interp(frac, b[interp0 * pchans], b[interp1 * pchans],
                                b[interp2 * pchans], b[interp3 * pchans]);
        } else {
            return linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
        }
    }
}

/**
 * @brief Calculate interpolation fraction and perform mono playback
 *
 * Combines fraction calculation and interpolation in one step for mono output.
 *
 * @param accuratehead Precise playhead position
 * @param direction Current playback direction
 * @param b Buffer pointer
 * @param pchans Buffer channel count
 * @param interp Interpolation type
 * @param directionorig Original recording direction
 * @param maxloop Maximum loop point
 * @param frames Total buffer frames
 * @param record Recording active flag
 * @return Interpolated mono sample
 */
inline double calculate_interpolation_fraction_and_osamp(
    double accuratehead,
    char direction,
    float* b,
    long pchans,
    interp_type_t interp,
    char directionorig,
    long maxloop,
    long frames,
    t_bool record) noexcept
{
    long playhead = static_cast<long>(trunc(accuratehead));
    double frac;
    long interp0, interp1, interp2, interp3;

    // Calculate interpolation fraction based on direction
    if (direction > 0) {
        frac = accuratehead - playhead;
    } else if (direction < 0) {
        frac = 1.0 - (accuratehead - playhead);
    } else {
        frac = 0.0;
    }

    // Get interpolation indices
    calculate_interp_indices_legacy(
        playhead, &interp0, &interp1, &interp2, &interp3,
        direction, directionorig >= 0, maxloop, frames - 1);

    // Perform playback interpolation and return result
    return perform_playback_interpolation(
        frac, b, interp0, interp1, interp2, interp3, pchans, interp, record);
}

/**
 * @brief Process ramps and fades for mono audio output
 *
 * Applies "switch and ramp" technique plus playback fades for smooth
 * transitions. Implements MSP's standard ramping algorithm.
 *
 * @param osamp1 Output sample (modified)
 * @param o1prev Previous sample storage
 * @param o1dif Sample difference for ramping
 * @param snrfade Switch-and-ramp fade position (0.0 to 1.0)
 * @param playfade Playback fade counter
 * @param globalramp Global ramp length in samples
 * @param snrramp Switch-and-ramp length in samples
 * @param snrtype Ramp curve type
 * @param playfadeflag Playback fade state flag
 * @param go Playback active flag
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 * @param loopdetermine Loop determination flag
 * @param record Recording active flag
 * @return Processed sample value
 */
inline double process_ramps_and_fades(
    double osamp1,
    double* o1prev,
    double* o1dif,
    double* snrfade,
    long* playfade,
    double globalramp,
    double snrramp,
    switchramp_type_t snrtype,
    char* playfadeflag,
    t_bool* go,
    t_bool* triginit,
    t_bool* jumpflag,
    t_bool* loopdetermine,
    t_bool record) noexcept
{
    if (globalramp) {
        // "Switch and Ramp" - http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
        if (*snrfade < 1.0) {
            if (*snrfade == 0.0) {
                *o1dif = *o1prev - osamp1;
            }
            osamp1 += ease_switchramp(*o1dif, *snrfade, snrtype);
            *snrfade += 1.0 / snrramp;
        }

        if (*playfade < globalramp) { // realtime ramps for play on/off
            osamp1 = ease_record(osamp1, (*playfadeflag > 0), globalramp, *playfade);
            (*playfade)++;
            if (*playfade >= globalramp) {
                process_playfade_state(
                    playfadeflag, go, triginit, jumpflag, loopdetermine,
                    playfade, snrfade, record);
            }
        }
    } else {
        process_playfade_state(
            playfadeflag, go, triginit, jumpflag, loopdetermine,
            playfade, snrfade, record);
    }

    return osamp1;
}

/**
 * @brief Calculate interpolation and get stereo output samples
 *
 * Performs interpolation for two channels (stereo). If buffer is mono,
 * duplicates channel 1 to channel 2.
 *
 * @param accuratehead Precise playhead position
 * @param direction Current playback direction
 * @param b Buffer pointer
 * @param pchans Buffer channel count
 * @param interp Interpolation type
 * @param directionorig Original recording direction
 * @param maxloop Maximum loop point
 * @param frames Total buffer frames
 * @param record Recording active flag
 * @param osamp1 Output channel 1 (modified)
 * @param osamp2 Output channel 2 (modified)
 */
inline void calculate_stereo_interpolation_and_osamp(
    double accuratehead,
    char direction,
    float* b,
    long pchans,
    interp_type_t interp,
    char directionorig,
    long maxloop,
    long frames,
    t_bool record,
    double* osamp1,
    double* osamp2) noexcept
{
    long playhead = static_cast<long>(trunc(accuratehead));
    double frac;
    long interp0, interp1, interp2, interp3;

    // Calculate interpolation fraction based on direction
    if (direction > 0) {
        frac = accuratehead - playhead;
    } else if (direction < 0) {
        frac = 1.0 - (accuratehead - playhead);
    } else {
        frac = 0.0;
    }

    // Get interpolation indices
    calculate_interp_indices_legacy(
        playhead, &interp0, &interp1, &interp2, &interp3,
        direction, directionorig >= 0, maxloop, frames - 1);

    // Perform playback interpolation for both channels
    *osamp1 = perform_playback_interpolation(
        frac, b, interp0, interp1, interp2, interp3, pchans, interp, record);
    *osamp2 = (pchans > 1) ? perform_playback_interpolation(
        frac, b, interp0 + 1, interp1 + 1, interp2 + 1, interp3 + 1, pchans, interp, record) : *osamp1;
}

/**
 * @brief Process ramps and fades for stereo audio output
 *
 * Applies ramping and fading to both stereo channels independently.
 *
 * @param osamp1 Output channel 1 (modified)
 * @param osamp2 Output channel 2 (modified)
 * @param o1prev Previous channel 1 storage
 * @param o2prev Previous channel 2 storage
 * @param o1dif Channel 1 difference
 * @param o2dif Channel 2 difference
 * @param snrfade Switch-and-ramp fade position
 * @param playfade Playback fade counter
 * @param globalramp Global ramp length
 * @param snrramp Switch-and-ramp length
 * @param snrtype Ramp curve type
 * @param playfadeflag Playback fade state flag
 * @param go Playback active flag
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 * @param loopdetermine Loop determination flag
 * @param record Recording active flag
 */
inline void process_stereo_ramps_and_fades(
    double* osamp1,
    double* osamp2,
    double* o1prev,
    double* o2prev,
    double* o1dif,
    double* o2dif,
    double* snrfade,
    long* playfade,
    double globalramp,
    double snrramp,
    switchramp_type_t snrtype,
    char* playfadeflag,
    t_bool* go,
    t_bool* triginit,
    t_bool* jumpflag,
    t_bool* loopdetermine,
    t_bool record) noexcept
{
    if (globalramp) {
        // "Switch and Ramp" - http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
        if (*snrfade < 1.0) {
            if (*snrfade == 0.0) {
                *o1dif = *o1prev - *osamp1;
                *o2dif = *o2prev - *osamp2;
            }
            *osamp1 += ease_switchramp(*o1dif, *snrfade, snrtype);
            *osamp2 += ease_switchramp(*o2dif, *snrfade, snrtype);
            *snrfade += 1.0 / snrramp;
        }

        if (*playfade < globalramp) { // realtime ramps for play on/off
            *osamp1 = ease_record(*osamp1, (*playfadeflag > 0), globalramp, *playfade);
            *osamp2 = ease_record(*osamp2, (*playfadeflag > 0), globalramp, *playfade);
            (*playfade)++;
            if (*playfade >= globalramp) {
                process_playfade_state(
                    playfadeflag, go, triginit, jumpflag, loopdetermine,
                    playfade, snrfade, record);
            }
        }
    } else {
        process_playfade_state(
            playfadeflag, go, triginit, jumpflag, loopdetermine,
            playfade, snrfade, record);
    }
}

/**
 * @brief Calculate interpolation for multichannel (poly) output
 *
 * Performs interpolation for arbitrary number of channels.
 * Handles channel wrapping when nchans > pchans.
 *
 * @param accuratehead Precise playhead position
 * @param direction Current playback direction
 * @param b Buffer pointer
 * @param pchans Buffer channel count
 * @param nchans Output channel count
 * @param interp Interpolation type
 * @param directionorig Original recording direction
 * @param maxloop Maximum loop point
 * @param frames Total buffer frames
 * @param record Recording active flag
 * @param osamp Output sample array (size nchans, modified)
 */
inline void calculate_poly_interpolation_and_osamp(
    double accuratehead,
    char direction,
    float* b,
    long pchans,
    long nchans,
    interp_type_t interp,
    char directionorig,
    long maxloop,
    long frames,
    t_bool record,
    double* osamp) noexcept
{
    long playhead = static_cast<long>(trunc(accuratehead));
    double frac;
    long interp0, interp1, interp2, interp3;

    // Calculate interpolation fraction based on direction
    if (direction > 0) {
        frac = accuratehead - playhead;
    } else if (direction < 0) {
        frac = 1.0 - (accuratehead - playhead);
    } else {
        frac = 0.0;
    }

    // Get interpolation indices
    calculate_interp_indices_legacy(
        playhead, &interp0, &interp1, &interp2, &interp3,
        direction, directionorig >= 0, maxloop, frames - 1);

    // Perform playback interpolation for all channels
    for (long i = 0; i < nchans; i++) {
        long chan_offset = i % pchans;
        osamp[i] = perform_playback_interpolation(
            frac, b + chan_offset, interp0 * pchans, interp1 * pchans,
            interp2 * pchans, interp3 * pchans, pchans, interp, record);
    }
}

/**
 * @brief Process ramps and fades for multichannel (poly) output
 *
 * Applies ramping and fading to all channels independently.
 *
 * @param osamp Output sample array (modified)
 * @param oprev Previous sample array
 * @param odif Difference array for ramping
 * @param nchans Number of channels
 * @param snrfade Switch-and-ramp fade position
 * @param playfade Playback fade counter
 * @param globalramp Global ramp length
 * @param snrramp Switch-and-ramp length
 * @param snrtype Ramp curve type
 * @param playfadeflag Playback fade state flag
 * @param go Playback active flag
 * @param triginit Trigger initialization flag
 * @param jumpflag Jump mode flag
 * @param loopdetermine Loop determination flag
 * @param record Recording active flag
 */
inline void process_poly_ramps_and_fades(
    double* osamp,
    double* oprev,
    double* odif,
    long nchans,
    double* snrfade,
    long* playfade,
    double globalramp,
    double snrramp,
    switchramp_type_t snrtype,
    char* playfadeflag,
    t_bool* go,
    t_bool* triginit,
    t_bool* jumpflag,
    t_bool* loopdetermine,
    t_bool record) noexcept
{
    if (globalramp) {
        // "Switch and Ramp" processing
        if (*snrfade < 1.0) {
            for (long i = 0; i < nchans; i++) {
                if (*snrfade == 0.0) {
                    odif[i] = oprev[i] - osamp[i];
                }
                osamp[i] += ease_switchramp(odif[i], *snrfade, snrtype);
            }
            *snrfade += 1.0 / snrramp;
        }

        if (*playfade < globalramp) { // realtime ramps for play on/off
            for (long i = 0; i < nchans; i++) {
                osamp[i] = ease_record(osamp[i], (*playfadeflag > 0), globalramp, *playfade);
            }
            (*playfade)++;
            if (*playfade >= globalramp) {
                process_playfade_state(
                    playfadeflag, go, triginit, jumpflag, loopdetermine,
                    playfade, snrfade, record);
            }
        }
    } else {
        process_playfade_state(
            playfadeflag, go, triginit, jumpflag, loopdetermine,
            playfade, snrfade, record);
    }
}

} // namespace karma

#endif // KARMA_PLAYBACK_DSP_HPP
