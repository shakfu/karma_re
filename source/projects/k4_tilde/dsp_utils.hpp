#ifndef KARMA_DSP_UTILS_HPP
#define KARMA_DSP_UTILS_HPP

// =============================================================================
// KARMA DSP UTILITIES - Audio Processing Helpers
// =============================================================================
// Utility functions for DSP calculations and buffer initialization.

namespace karma {

/**
 * @brief Calculate sync outlet phase output
 *
 * Writes audio sample to output and optionally calculates normalized
 * phase position for sync outlet.
 *
 * @param osamp1 Output sample value
 * @param o1prev Previous sample storage (updated)
 * @param out1 Audio output pointer (incremented after write)
 * @param syncoutlet Whether sync outlet is enabled
 * @param outPh Phase output pointer (incremented after write if enabled)
 * @param accuratehead Current playhead position
 * @param minloop Loop minimum point
 * @param maxloop Loop maximum point
 * @param directionorig Original recording direction
 * @param frames Total buffer frames
 * @param setloopsize Loop size (may be recalculated)
 */
inline void calculate_sync_output(
    double osamp1,
    double* o1prev,
    double** out1,
    char syncoutlet,
    double** outPh,
    double accuratehead,
    double minloop,
    double maxloop,
    char directionorig,
    long frames,
    double setloopsize) noexcept
{
    *o1prev = osamp1;
    *(*out1)++ = osamp1;

    if (syncoutlet) {
        setloopsize = maxloop - minloop;
        *(*outPh)++ = (directionorig >= 0)
            ? ((accuratehead - minloop) / setloopsize)
            : ((accuratehead - (frames - setloopsize)) / setloopsize);
    }
}

/**
 * @brief Apply iPoke linear interpolation over buffer range
 *
 * Fills buffer region with linearly interpolated values when recording
 * at speeds != 1.0. Handles both forward and reverse directions.
 *
 * @param b Buffer to write to
 * @param pchans Number of interleaved channels
 * @param start_idx Starting buffer index
 * @param end_idx Ending buffer index
 * @param writeval1 Current write value (modified during iteration)
 * @param coeff1 Interpolation coefficient per sample
 * @param direction Playback direction (>0 forward, <0 reverse)
 */
inline void apply_ipoke_interpolation(
    float* b,
    long pchans,
    long start_idx,
    long end_idx,
    double* writeval1,
    double coeff1,
    char direction) noexcept
{
    if (direction > 0) {
        for (long i = start_idx; i < end_idx; i++) {
            *writeval1 += coeff1;
            b[i * pchans] = static_cast<float>(*writeval1);
        }
    } else {
        for (long i = start_idx; i > end_idx; i--) {
            *writeval1 -= coeff1;
            b[i * pchans] = static_cast<float>(*writeval1);
        }
    }
}

/**
 * @brief Initialize buffer properties from Max buffer object
 *
 * Reads buffer metadata (channels, frames, sample rate) and initializes
 * the karma object's buffer group fields. Also calculates sample rate
 * scaling factor.
 *
 * @param x Karma object
 * @param buf Max buffer object to read from
 */
inline void init_buffer_properties(t_karma* x, t_buffer_obj* buf) noexcept
{
    x->buffer.bchans  = buffer_getchannelcount(buf);
    x->buffer.bframes = buffer_getframecount(buf);
    x->buffer.bmsr    = buffer_getmillisamplerate(buf);
    x->buffer.bsr     = buffer_getsamplerate(buf);
    x->buffer.nchans  = (x->buffer.bchans < x->buffer.ochans)
        ? x->buffer.bchans
        : x->buffer.ochans;  // MIN
    x->timing.srscale = x->buffer.bsr / x->timing.ssr;
}

} // namespace karma

#endif // KARMA_DSP_UTILS_HPP
