#ifndef KARMA_LOOP_CONFIG_HPP
#define KARMA_LOOP_CONFIG_HPP

#include "dsp_utils.hpp"

// =============================================================================
// KARMA LOOP CONFIGURATION - Loop Point Setting and Validation
// =============================================================================
// Functions for parsing and validating loop point parameters from Max messages.
// Handles setloop message argument parsing and buffer value calculations.

namespace karma {

/**
 * @brief Process and validate buffer loop values
 *
 * Calculates and validates loop boundaries based on input parameters. Handles
 * normalization from different input formats (phase, samples, milliseconds),
 * validates ranges, enforces minimum loop size, and updates karma object state.
 *
 * Input Formats:
 * - loop_points_flag 0: Phase (normalized 0.0-1.0)
 * - loop_points_flag 1: Samples (absolute sample count)
 * - loop_points_flag 2: Milliseconds (time-based)
 *
 * Validation:
 * - Ensures low < high
 * - Clamps to buffer boundaries
 * - Enforces minimum loop size (vectorsize)
 * - Adjusts if needed to maintain valid loop
 *
 * @param x Karma object (modified - updates minloop, maxloop, startloop, endloop)
 * @param templow Lower loop boundary (format determined by loop_points_flag)
 * @param temphigh Upper loop boundary (format determined by loop_points_flag)
 * @param loop_points_flag Input format: 0=phase, 1=samples, 2=milliseconds
 * @param caller Caller identification (true=buf_change, false=setloop)
 */
inline void process_buf_values_internal(
    t_karma* x,
    double templow,
    double temphigh,
    long loop_points_flag,
    t_bool caller) noexcept
{
    t_symbol*     caller_sym = nullptr;
    t_buffer_obj* buf;
    long          bframesm1;
    double        bframesms, bvsnorm, bvsnorm05;
    double        low, lowtemp, high, hightemp;
    low = templow;
    high = temphigh;

    if (caller) { // only if called from 'karma_buf_change_internal()'
        buf = buffer_ref_getobject(x->buffer.buf);
        init_buffer_properties(x, buf);
        caller_sym = gensym("set");
    } else {
        caller_sym = gensym("setloop");
    }

    bframesm1 = (x->buffer.bframes - 1);
    bframesms = static_cast<double>(bframesm1) / x->buffer.bmsr;
    bvsnorm = x->timing.vsnorm * (x->buffer.bsr / static_cast<double>(x->buffer.bframes));
    bvsnorm05 = bvsnorm * 0.5;
    x->timing.bvsnorm = bvsnorm;

    // By this stage, if LOW < 0, it has not been set and should default to 0
    if (low < 0.)
        low = 0.;

    if (loop_points_flag == 0) { // PHASE
        if (high < 0.)
            high = 1.; // already normalised 0..1
    } else if (loop_points_flag == 1) { // SAMPLES
        if (high < 0.)
            high = 1.;
        else
            high = high / static_cast<double>(bframesm1);

        if (low > 0.)
            low = low / static_cast<double>(bframesm1);
    } else { // MILLISECONDS (default)
        if (high < 0.)
            high = 1.;
        else
            high = high / bframesms;

        if (low > 0.)
            low = low / bframesms;
    }

    // Normalize and sort
    lowtemp = low;
    hightemp = high;
    low = MIN(lowtemp, hightemp);
    high = MAX(lowtemp, hightemp);

    // Validate ranges
    if (low > 1.) {
        object_warn(
            reinterpret_cast<t_object*>(x),
            "loop minimum cannot be greater than available buffer~ size, "
            "setting to buffer~ size minus vectorsize");
        low = 1. - bvsnorm;
    }
    if (high > 1.) {
        object_warn(
            reinterpret_cast<t_object*>(x),
            "loop maximum cannot be greater than available buffer~ size, "
            "setting to buffer~ size");
        high = 1.;
    }

    // Check minimum loop size
    if ((high - low) < bvsnorm) {
        if ((high - low) == 0.) {
            object_warn(
                reinterpret_cast<t_object*>(x),
                "loop size cannot be zero, ignoring %s command",
                caller_sym);
            return;
        } else {
            object_warn(
                reinterpret_cast<t_object*>(x),
                "loop size cannot be this small, minimum is vectorsize "
                "internally (currently using %.0f samples)",
                x->timing.vs);
            if ((low - bvsnorm05) < 0.) {
                low = 0.;
                high = bvsnorm;
            } else if ((high + bvsnorm05) > 1.) {
                high = 1.;
                low = 1. - bvsnorm;
            } else {
                low = low - bvsnorm05;
                high = high + bvsnorm05;
            }
        }
    }

    low = CLAMP(low, 0., 1.);
    high = CLAMP(high, 0., 1.);

    x->loop.minloop = x->loop.startloop = low * bframesm1;
    x->loop.maxloop = x->loop.endloop = high * bframesm1;

    // Update selection (external functions)
    karma_select_size(x, x->timing.selection);
    karma_select_start(x, x->timing.selstart);
}

/**
 * @brief Parse setloop message arguments
 *
 * Parses arguments from the "setloop" Max message and extracts loop boundaries
 * and format specification. Handles 1-3 arguments with flexible format:
 * - 1 arg: low point (high defaults to max, format defaults to ms)
 * - 2 args: low high (format defaults to ms) OR low format (high defaults to max)
 * - 3 args: low high format
 *
 * Format Specifications:
 * - "phase"/"ph"/0: Normalized 0.0-1.0
 * - "samples"/"samps"/1: Sample count
 * - "milliseconds"/"ms"/2: Time in ms (default)
 *
 * @param x Karma object
 * @param s Symbol (message name for error reporting)
 * @param argc Argument count
 * @param argv Argument vector
 */
inline void process_setloop_internal(
    t_karma* x,
    t_symbol* s,
    short argc,
    t_atom* argv) noexcept
{
    t_bool    callerid = false;
    t_symbol* loop_points_sym = nullptr;
    long      loop_points_flag = 2; // Default: milliseconds
    double    templow = -1.;
    double    temphigh = -1.;
    double    temphightemp;

    // Parse argument 3 (format specification)
    if (argc >= 3) {
        if (argc > 3)
            object_warn(
                reinterpret_cast<t_object*>(x),
                "too many arguments for %s message, truncating to first three args",
                s->s_name);

        if (atom_gettype(argv + 2) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 2);
            if ((loop_points_sym == gensym("phase")) || (loop_points_sym == gensym("PHASE"))
                || (loop_points_sym == gensym("ph")))
                loop_points_flag = 0;
            else if ((loop_points_sym == gensym("samples")) || (loop_points_sym == gensym("SAMPLES"))
                     || (loop_points_sym == gensym("samps")))
                loop_points_flag = 1;
            else
                loop_points_flag = 2;
        } else if (atom_gettype(argv + 2) == A_LONG) {
            loop_points_flag = atom_getlong(argv + 2);
        } else if (atom_gettype(argv + 2) == A_FLOAT) {
            loop_points_flag = static_cast<long>(atom_getfloat(argv + 2));
        } else {
            object_warn(
                reinterpret_cast<t_object*>(x),
                "%s message does not understand arg no.3, using milliseconds for args 1 & 2",
                s->s_name);
            loop_points_flag = 2;
        }

        loop_points_flag = CLAMP(loop_points_flag, 0, 2);
    }

    // Parse argument 2 (high point or format)
    if (argc >= 2) {
        if (atom_gettype(argv + 1) == A_FLOAT) {
            temphigh = atom_getfloat(argv + 1);
            if (temphigh < 0.) {
                object_warn(
                    reinterpret_cast<t_object*>(x),
                    "loop maximum cannot be less than 0., resetting");
            }
        } else if (atom_gettype(argv + 1) == A_LONG) {
            temphigh = static_cast<double>(atom_getlong(argv + 1));
            if (temphigh < 0.) {
                object_warn(
                    reinterpret_cast<t_object*>(x),
                    "loop maximum cannot be less than 0., resetting");
            }
        } else if ((atom_gettype(argv + 1) == A_SYM) && (argc < 3)) {
            loop_points_sym = atom_getsym(argv + 1);
            if ((loop_points_sym == gensym("phase")) || (loop_points_sym == gensym("PHASE"))
                || (loop_points_sym == gensym("ph")))
                loop_points_flag = 0;
            else if ((loop_points_sym == gensym("samples")) || (loop_points_sym == gensym("SAMPLES"))
                     || (loop_points_sym == gensym("samps")))
                loop_points_flag = 1;
            else if ((loop_points_sym == gensym("milliseconds")) || (loop_points_sym == gensym("MS"))
                     || (loop_points_sym == gensym("ms")))
                loop_points_flag = 2;
            else {
                object_warn(
                    reinterpret_cast<t_object*>(x),
                    "%s message does not understand arg no.2, setting to milliseconds",
                    s->s_name);
                loop_points_flag = 2;
            }
        } else {
            object_warn(
                reinterpret_cast<t_object*>(x),
                "%s message does not understand arg no.2, setting unit to maximum",
                s->s_name);
        }
    }

    // Parse argument 1 (low point)
    if (argc >= 1) {
        if (atom_gettype(argv + 0) == A_FLOAT) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = atom_getfloat(argv + 0);
                templow = temphightemp;
            } else {
                templow = atom_getfloat(argv + 0);
            }
        } else if (atom_gettype(argv + 0) == A_LONG) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = static_cast<double>(atom_getlong(argv + 0));
                templow = temphightemp;
            } else {
                templow = static_cast<double>(atom_getlong(argv + 0));
            }
        } else if (atom_gettype(argv + 0) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 0);
            if ((loop_points_sym == gensym("phase")) || (loop_points_sym == gensym("PHASE"))
                || (loop_points_sym == gensym("ph")))
                loop_points_flag = 0;
            else if ((loop_points_sym == gensym("samples")) || (loop_points_sym == gensym("SAMPLES"))
                     || (loop_points_sym == gensym("samps")))
                loop_points_flag = 1;
            else if ((loop_points_sym == gensym("milliseconds")) || (loop_points_sym == gensym("MS"))
                     || (loop_points_sym == gensym("ms")))
                loop_points_flag = 2;
            else {
                object_warn(
                    reinterpret_cast<t_object*>(x),
                    "%s message does not understand arg no.1, setting to milliseconds",
                    s->s_name);
                loop_points_flag = 2;
            }
        } else {
            object_warn(
                reinterpret_cast<t_object*>(x),
                "%s message does not understand arg no.1, setting to 0.",
                s->s_name);
        }
    }

    process_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

} // namespace karma

#endif // KARMA_LOOP_CONFIG_HPP
