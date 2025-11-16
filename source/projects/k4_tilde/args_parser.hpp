#ifndef KARMA_ARGS_PARSER_HPP
#define KARMA_ARGS_PARSER_HPP

// =============================================================================
// KARMA ARGS PARSER - Message Argument Parsing and Validation
// =============================================================================
// Functions for parsing Max/MSP message arguments and validating buffer names.

namespace karma {

/**
 * @brief Validate and set buffer reference
 *
 * Validates that a buffer~ exists and sets up the buffer reference.
 * Used when receiving 'set' messages or other buffer name changes.
 *
 * @param x Karma object
 * @param bufname Buffer name symbol to validate
 * @return true if buffer is valid and set successfully, false otherwise
 */
inline t_bool validate_buffer(t_karma* x, t_symbol* bufname) noexcept
{
    t_buffer_obj* buf_temp;

    if (bufname == gensym("")) {
        object_error((t_object*)x, "requires a valid buffer~ declaration (none found)");
        return false;
    }

    x->buffer.bufname_temp = bufname;

    if (!x->buffer.buf_temp) {
        x->buffer.buf_temp = buffer_ref_new((t_object*)x, bufname);
    } else {
        buffer_ref_set(x->buffer.buf_temp, bufname);
    }

    buf_temp = buffer_ref_getobject(x->buffer.buf_temp);

    if (buf_temp == NULL) {
        object_warn(
            (t_object*)x, "cannot find any buffer~ named %s, ignoring", bufname->s_name);
        x->buffer.buf_temp = 0;
        object_free(x->buffer.buf_temp);
        return false;
    }

    x->buffer.buf_temp = 0;
    object_free(x->buffer.buf_temp);

    // Set up the main buffer reference
    x->buffer.bufname = bufname;
    if (!x->buffer.buf) {
        x->buffer.buf = buffer_ref_new((t_object*)x, bufname);
    } else {
        buffer_ref_set(x->buffer.buf, bufname);
    }

    return true;
}

/**
 * @brief Parse loop points unit symbol
 *
 * Converts symbolic unit names (phase/samples/milliseconds) to integer flags.
 * Supports multiple variations: "phase"/"PHASE"/"ph", "samples"/"SAMPLES"/"samps",
 * "milliseconds"/"MS"/"ms"
 *
 * @param loop_points_sym Symbol representing unit type
 * @param loop_points_flag Output flag (0=phase, 1=samples, 2=milliseconds)
 */
inline void parse_loop_points_sym(t_symbol* loop_points_sym, long* loop_points_flag) noexcept
{
    if (loop_points_sym == gensym("")) {
        *loop_points_flag = 2;
    } else if (
        (loop_points_sym == gensym("phase")) || (loop_points_sym == gensym("PHASE"))
                                             || (loop_points_sym == gensym("ph"))) {
        *loop_points_flag = 0;
    } else if (
        (loop_points_sym == gensym("samples")) || (loop_points_sym == gensym("SAMPLES"))
                                                || (loop_points_sym == gensym("samps"))) {
        *loop_points_flag = 1;
    } else if (
        (loop_points_sym == gensym("milliseconds")) || (loop_points_sym == gensym("MS"))
                                                     || (loop_points_sym == gensym("ms"))) {
        *loop_points_flag = 2;
    } else {
        *loop_points_flag = 2; // default to milliseconds
    }
}

/**
 * @brief Parse numeric argument (float or long) to double
 *
 * Extracts numeric value from Max atom, handling both float and long types.
 *
 * @param arg Max atom to parse
 * @param value Output value
 */
inline void parse_numeric_arg(t_atom* arg, double* value) noexcept
{
    if (atom_gettype(arg) == A_FLOAT) {
        *value = atom_getfloat(arg);
    } else if (atom_gettype(arg) == A_LONG) {
        *value = static_cast<double>(atom_getlong(arg));
    }
}

/**
 * @brief Process argc/argv arguments for loop points
 *
 * Parses up to 4 arguments to extract loop point values and unit type.
 * Arguments are processed in reverse order (4th, 3rd, 2nd) with special
 * fallback logic for different argument patterns.
 *
 * Expected patterns:
 * - 2 args: low high (default: milliseconds)
 * - 3 args: low high unit OR low high OR low unit
 * - 4 args: low high unit extra
 *
 * @param x Karma object
 * @param s Message symbol
 * @param argc Argument count
 * @param argv Argument vector
 * @param templow Output low loop point
 * @param temphigh Output high loop point
 * @param loop_points_flag Output unit flag (0=phase, 1=samples, 2=milliseconds)
 */
inline void process_argc_args(
    t_karma* x, t_symbol* s, short argc, t_atom* argv, double* templow, double* temphigh,
    long* loop_points_flag) noexcept
{
    t_symbol* loop_points_sym = 0;
    double    temphightemp;

    // Initialize defaults
    *loop_points_flag = 2; // milliseconds
    *templow = -1.0;
    *temphigh = -1.0;

    // Process argument 4 (index 3) - loop points type
    if (argc >= 4) {
        if (atom_gettype(argv + 3) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 3);
            parse_loop_points_sym(loop_points_sym, loop_points_flag);
        } else if (atom_gettype(argv + 3) == A_LONG) {
            *loop_points_flag = atom_getlong(argv + 3);
        } else if (atom_gettype(argv + 3) == A_FLOAT) {
            *loop_points_flag = static_cast<long>(atom_getfloat(argv + 3));
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.4, using milliseconds "
                "for args 2 & 3",
                s->s_name);
            *loop_points_flag = 2;
        }
        *loop_points_flag = CLAMP(*loop_points_flag, 0, 2);
    }

    // Process argument 3 (index 2) - high value or loop points type
    if (argc >= 3) {
        if (atom_gettype(argv + 2) == A_FLOAT || atom_gettype(argv + 2) == A_LONG) {
            parse_numeric_arg(argv + 2, temphigh);
            if (*temphigh < 0.) {
                object_warn(
                    (t_object*)x, "loop maximum cannot be less than 0., resetting");
            }
        } else if (atom_gettype(argv + 2) == A_SYM && argc < 4) {
            loop_points_sym = atom_getsym(argv + 2);
            parse_loop_points_sym(loop_points_sym, loop_points_flag);
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.3, setting unit to "
                "maximum",
                s->s_name);
        }
    }

    // Process argument 2 (index 1) - low value or special handling
    if (argc >= 2) {
        if (atom_gettype(argv + 1) == A_FLOAT || atom_gettype(argv + 1) == A_LONG) {
            if (*temphigh < 0.) {
                temphightemp = *temphigh;
                parse_numeric_arg(argv + 1, temphigh);
                *templow = temphightemp;
            } else {
                parse_numeric_arg(argv + 1, templow);
                if (*templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    *templow = 0.;
                }
            }
        } else if (atom_gettype(argv + 1) == A_SYM) {
            loop_points_sym = atom_getsym(argv + 1);
            if (loop_points_sym == gensym("")) {
                *loop_points_flag = 2;
            } else if (loop_points_sym == gensym("originalloop")) {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand 'buffername' followed by "
                    "%s message, ignoring",
                    s->s_name, loop_points_sym->s_name);
                object_warn(
                    (t_object*)x,
                    "(the %s message cannot be used whilst changing buffer~ "
                    "reference",
                    loop_points_sym->s_name);
                object_warn(
                    (t_object*)x, "use %s %s message or just %s message instead)",
                    gensym("setloop")->s_name, gensym("originalloop")->s_name,
                    gensym("resetloop")->s_name);
                // Set flag to indicate early return needed
                *templow = KARMA_SENTINEL_VALUE; // Special flag value
                return;
            } else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.2, setting loop "
                    "points to minimum (and maximum)",
                    s->s_name);
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.2, setting loop points "
                "to defaults",
                s->s_name);
        }
    }
}

} // namespace karma

#endif // KARMA_ARGS_PARSER_HPP
