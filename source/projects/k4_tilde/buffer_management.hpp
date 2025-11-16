#ifndef KARMA_BUFFER_MANAGEMENT_HPP
#define KARMA_BUFFER_MANAGEMENT_HPP

#include "dsp_utils.hpp"
#include "args_parser.hpp"
#include "loop_config.hpp"

// =============================================================================
// KARMA BUFFER MANAGEMENT - Buffer Setup and Modification Handling
// =============================================================================
// Functions for managing buffer~ references, handling buffer changes, and
// responding to buffer modification notifications.

namespace karma {

/**
 * @brief Setup buffer reference and initialize state
 *
 * Called by karma_dsp64() to set up the buffer~ reference. Creates or updates
 * the buffer reference, initializes buffer properties, and resets playback state.
 *
 * @param x Karma object (modified - updates buffer properties and state)
 * @param s Buffer name symbol
 */
inline void setup_buffer(t_karma* x, t_symbol* s) noexcept
{
    t_buffer_obj* buf;
    x->buffer.bufname = s;

    if (!x->buffer.buf)
        x->buffer.buf = buffer_ref_new(reinterpret_cast<t_object*>(x), s);
    else
        buffer_ref_set(x->buffer.buf, s);

    buf = buffer_ref_getobject(x->buffer.buf);

    if (buf == NULL) {
        x->buffer.buf = nullptr;
    } else {
        x->state.directionorig = 0;
        x->timing.maxhead = x->timing.playhead = 0.0;
        x->timing.recordhead = -1;
        init_buffer_properties(x, buf);
        x->timing.bvsnorm = x->timing.vsnorm
            * (x->buffer.bsr / static_cast<double>(x->buffer.bframes));
        x->loop.minloop = x->loop.startloop = 0.0;
        x->loop.maxloop = x->loop.endloop = (x->buffer.bframes - 1);
        x->timing.selstart = 0.0;
        x->timing.selection = 1.0;
    }
}

/**
 * @brief Handle buffer modification notifications
 *
 * Called when the buffer~ contents or properties are modified. Updates karma
 * state to match new buffer properties (sample rate, channel count, frame count).
 * Resets loop boundaries and selection to match new buffer size.
 *
 * @param x Karma object (modified - updates buffer properties and loop boundaries)
 * @param b Buffer object that was modified
 */
inline void handle_buffer_modify(t_karma* x, t_buffer_obj* b) noexcept
{
    double modbsr, modbmsr;
    long   modchans, modframes;

    if (b) {
        modbsr = buffer_getsamplerate(b);
        modchans = buffer_getchannelcount(b);
        modframes = buffer_getframecount(b);
        modbmsr = buffer_getmillisamplerate(b);

        if (((x->buffer.bchans != modchans) || (x->buffer.bframes != modframes))
            || (x->buffer.bmsr != modbmsr)) {
            x->buffer.bsr = modbsr;
            x->buffer.bmsr = modbmsr;
            x->timing.srscale = modbsr / x->timing.ssr;
            x->buffer.bframes = modframes;
            x->buffer.bchans = modchans;
            x->buffer.nchans = (modchans < x->buffer.ochans) ? modchans : x->buffer.ochans;
            x->loop.minloop = x->loop.startloop = 0.0;
            x->loop.maxloop = x->loop.endloop = (x->buffer.bframes - 1);
            x->timing.bvsnorm = x->timing.vsnorm * (modbsr / static_cast<double>(modframes));

            karma_select_size(x, x->timing.selection);
            karma_select_start(x, x->timing.selstart);
        }
    }
}

/**
 * @brief Process buffer change internal
 *
 * Internal handler for "set" message (deferred execution). Changes the associated
 * buffer~ and optionally sets new loop points.
 *
 * Arguments:
 * - argv[0]: buffer name (required, symbol)
 * - argv[1-3]: optional loop point arguments (see process_argc_args)
 *
 * @param x Karma object (modified - updates buffer reference and state)
 * @param s Message symbol (for error reporting)
 * @param argc Argument count
 * @param argv Argument vector
 */
inline void process_buffer_change_internal(
    t_karma* x,
    t_symbol* s,
    short argc,
    t_atom* argv) noexcept
{
    t_bool    callerid = true;
    t_symbol* bufname;
    long      loop_points_flag;
    double    templow, temphigh;

    // Get buffer name from first argument
    bufname = atom_getsym(argv + 0);

    // Validate buffer and set up references
    if (!validate_buffer(x, bufname)) {
        return;
    }

    // Reset player state
    x->state.directionorig = 0;
    x->timing.maxhead = x->timing.playhead = 0.0;
    x->timing.recordhead = -1;

    // Process arguments to extract loop points and settings
    process_argc_args(x, s, argc, argv, &templow, &temphigh, &loop_points_flag);

    // Check for early return flag from ps_originalloop handling
    if (templow == KARMA_SENTINEL_VALUE) {
        return;
    }

    // Apply the buffer values
    process_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

/**
 * @brief Validate and prepare buffer change arguments
 *
 * Handles "set" message from Max. Validates arguments and defers execution
 * to process_buffer_change_internal().
 *
 * Arguments:
 * - 1st arg: buffer name (required, symbol)
 * - Additional args: optional loop points (see process_argc_args)
 *
 * @param x Karma object
 * @param s Message symbol (for error reporting)
 * @param ac Argument count
 * @param av Argument vector
 */
inline void prepare_buffer_change(
    t_karma* x,
    t_symbol* s,
    short ac,
    t_atom* av) noexcept
{
    t_atom store_av[4];
    short  i, j, a;
    a = ac;

    if (a <= 0) {
        object_error(
            reinterpret_cast<t_object*>(x),
            "%s message must be followed by argument(s) (it does nothing alone)",
            s->s_name);
        return;
    }

    if (atom_gettype(av + 0) != A_SYM) {
        object_error(
            reinterpret_cast<t_object*>(x),
            "first argument to %s message must be a symbol (associated buffer~ name)",
            s->s_name);
        return;
    }

    if (a > 4) {
        object_warn(
            reinterpret_cast<t_object*>(x),
            "too many arguments for %s message, truncating to first four args",
            s->s_name);
        a = 4;

        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }
    } else {
        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }

        for (j = i; j < 4; j++) {
            atom_setsym(store_av + j, gensym(""));
        }
    }

    defer(x, reinterpret_cast<method>(process_buffer_change_internal), s, ac, store_av);
}

} // namespace karma

#endif // KARMA_BUFFER_MANAGEMENT_HPP
