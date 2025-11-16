#ifndef KARMA_STEREO_RECORDING_HPP
#define KARMA_STEREO_RECORDING_HPP

// =============================================================================
// KARMA STEREO RECORDING - Stereo iPoke Recording Functions
// =============================================================================
// Stereo versions of iPoke recording functions. These mirror the mono versions
// but handle two channels simultaneously with interleaved buffer access.

namespace karma {

/**
 * @brief Process stereo iPoke recording with interpolation
 *
 * Stereo version of iPoke recording. Records two channels with linear
 * interpolation/averaging to handle variable speeds. Mirrors the mono
 * implementation but processes both channels in parallel.
 *
 * @param b Buffer pointer
 * @param pchans Buffer channel count (must be >= 2 for stereo)
 * @param playhead Current playback position
 * @param recordhead Record head position (modified)
 * @param recin1 Input sample channel 1
 * @param recin2 Input sample channel 2
 * @param overdubamp Overdub amplitude (unused in current implementation)
 * @param globalramp Global ramp length
 * @param recordfade Recording fade counter
 * @param recfadeflag Recording fade flag
 * @param pokesteps iPoke steps counter (modified)
 * @param writeval1 Write value accumulator channel 1 (modified)
 * @param writeval2 Write value accumulator channel 2 (modified)
 * @param dirt Buffer modified flag (set to 1)
 */
inline void process_ipoke_recording_stereo(
    float* b,
    long pchans,
    long playhead,
    long* recordhead,
    double recin1,
    double recin2,
    double overdubamp,
    double globalramp,
    long recordfade,
    char recfadeflag,
    double* pokesteps,
    double* writeval1,
    double* writeval2,
    t_bool* dirt) noexcept
{
    long   i;
    double recplaydif, coeff1, coeff2;

    // Handle first record head initialization
    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *writeval2 += recin2;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *writeval2 = *writeval2 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = static_cast<float>(*writeval1);
        if (pchans > 1) {
            b[*recordhead * pchans + 1] = static_cast<float>(*writeval2);
        }
        recplaydif = static_cast<double>(playhead - *recordhead);
        if (recplaydif > 0) { // linear-interpolation for speed > 1x
            coeff1 = (recin1 - *writeval1) / recplaydif;
            coeff2 = (recin2 - *writeval2) / recplaydif;
            for (i = *recordhead + 1; i < playhead; i++) {
                *writeval1 += coeff1;
                *writeval2 += coeff2;
                b[i * pchans] = static_cast<float>(*writeval1);
                if (pchans > 1) {
                    b[i * pchans + 1] = static_cast<float>(*writeval2);
                }
            }
        } else {
            coeff1 = (recin1 - *writeval1) / recplaydif;
            coeff2 = (recin2 - *writeval2) / recplaydif;
            for (i = *recordhead - 1; i > playhead; i--) {
                *writeval1 -= coeff1;
                *writeval2 -= coeff2;
                b[i * pchans] = static_cast<float>(*writeval1);
                if (pchans > 1) {
                    b[i * pchans + 1] = static_cast<float>(*writeval2);
                }
            }
        }
        *writeval1 = recin1;
        *writeval2 = recin2;
    }
    *recordhead = playhead;
    *dirt = 1;
}

/**
 * @brief Process stereo iPoke recording during initial loop creation
 *
 * Stereo version of initial loop iPoke recording. Handles direction reversals
 * and wrap-around logic for both channels simultaneously. This is the stereo
 * equivalent of process_initial_loop_ipoke_recording().
 *
 * @param b Buffer pointer
 * @param pchans Buffer channel count (must be >= 2 for stereo)
 * @param recordhead Record head position (modified)
 * @param playhead Current playback position
 * @param recin1 Input sample channel 1
 * @param recin2 Input sample channel 2
 * @param pokesteps iPoke steps accumulator (modified)
 * @param writeval1 Write value accumulator channel 1 (modified)
 * @param writeval2 Write value accumulator channel 2 (modified)
 * @param direction Current playback direction
 * @param directionorig Original recording direction
 * @param maxhead Maximum head position reached during recording
 * @param frames Total buffer frame count
 */
inline void process_initial_loop_ipoke_recording_stereo(
    float* b,
    long pchans,
    long* recordhead,
    long playhead,
    double recin1,
    double recin2,
    double* pokesteps,
    double* writeval1,
    double* writeval2,
    char direction,
    char directionorig,
    long maxhead,
    long frames) noexcept
{
    long   i;
    double recplaydif, coeff1, coeff2;

    if (*recordhead < 0) {
        *recordhead = playhead;
        *pokesteps = 0.0;
    }

    if (*recordhead == playhead) {
        *writeval1 += recin1;
        *writeval2 += recin2;
        *pokesteps += 1.0;
    } else {
        if (*pokesteps > 1.0) { // linear-averaging for speed < 1x
            *writeval1 = *writeval1 / *pokesteps;
            *writeval2 = *writeval2 / *pokesteps;
            *pokesteps = 1.0;
        }
        b[*recordhead * pchans] = static_cast<float>(*writeval1);
        if (pchans > 1) {
            b[*recordhead * pchans + 1] = static_cast<float>(*writeval2);
        }
        recplaydif = static_cast<double>(playhead - *recordhead); // linear-interp for speed > 1x

        if (direction != directionorig) {
            if (directionorig >= 0) {
                if (recplaydif > 0) {
                    if (recplaydif > (maxhead * 0.5)) {
                        recplaydif -= maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i >= 0; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                        for (i = maxhead; i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    }
                } else {
                    if ((-recplaydif) > (maxhead * 0.5)) {
                        recplaydif += maxhead;
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < (maxhead + 1); i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                        for (i = 0; i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    }
                }
            } else {
                if (recplaydif > 0) {
                    if (recplaydif > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif -= ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i >= maxhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                        for (i = (frames - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < playhead; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    }
                } else {
                    if ((-recplaydif) > (((frames - 1) - (maxhead)) * 0.5)) {
                        recplaydif += ((frames - 1) - (maxhead));
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead + 1); i < frames; i++) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                        for (i = maxhead; i > playhead; i--) {
                            *writeval1 += coeff1;
                            *writeval2 += coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    } else {
                        coeff1 = (recin1 - *writeval1) / recplaydif;
                        coeff2 = (recin2 - *writeval2) / recplaydif;
                        for (i = (*recordhead - 1); i > playhead; i--) {
                            *writeval1 -= coeff1;
                            *writeval2 -= coeff2;
                            b[i * pchans] = static_cast<float>(*writeval1);
                            if (pchans > 1) {
                                b[i * pchans + 1] = static_cast<float>(*writeval2);
                            }
                        }
                    }
                }
            }
        } else {
            if (recplaydif > 0) { // linear-interpolation for speed > 1x
                coeff1 = (recin1 - *writeval1) / recplaydif;
                coeff2 = (recin2 - *writeval2) / recplaydif;
                for (i = *recordhead + 1; i < playhead; i++) {
                    *writeval1 += coeff1;
                    *writeval2 += coeff2;
                    b[i * pchans] = static_cast<float>(*writeval1);
                    if (pchans > 1) {
                        b[i * pchans + 1] = static_cast<float>(*writeval2);
                    }
                }
            } else {
                coeff1 = (recin1 - *writeval1) / recplaydif;
                coeff2 = (recin2 - *writeval2) / recplaydif;
                for (i = *recordhead - 1; i > playhead; i--) {
                    *writeval1 -= coeff1;
                    *writeval2 -= coeff2;
                    b[i * pchans] = static_cast<float>(*writeval1);
                    if (pchans > 1) {
                        b[i * pchans + 1] = static_cast<float>(*writeval2);
                    }
                }
            }
        }
        *writeval1 = recin1;
        *writeval2 = recin2;
    }
}

} // namespace karma

#endif // KARMA_STEREO_RECORDING_HPP
