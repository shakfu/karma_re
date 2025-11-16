#ifndef KARMA_LOOP_BOUNDS_HPP
#define KARMA_LOOP_BOUNDS_HPP

#include "fade_engine.hpp"

// =============================================================================
// KARMA LOOP BOUNDS - Loop Boundary Processing
// =============================================================================
// Functions for handling loop boundary wrapping, jumping, and cleanup.

namespace karma {

/**
 * @brief Clean up recording state when crossing loop boundaries
 *
 * Applies fades and resets state when the playhead wraps or jumps
 * across loop boundaries during recording.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Current playhead position
 * @param direction Playback direction (-1, 0, 1)
 * @param use_ease_on If true, use fade-in; if false, use fade-out
 * @param ease_pos Position for fade-out when use_ease_on is false
 */
inline void process_recording_cleanup(
    t_karma* x,
    float* b,
    double accuratehead,
    char direction,
    t_bool use_ease_on,
    double ease_pos) noexcept
{
    x->fade.snrfade = 0.0;

    if (x->state.record) {
        if (x->fade.globalramp) {
            if (use_ease_on) {
                ease_buffer_fadein(
                    x->buffer.bframes - 1,
                    b,
                    x->buffer.nchans,
                    static_cast<long>(accuratehead),
                    x->timing.recordhead,
                    direction,
                    x->fade.globalramp);
            } else {
                ease_buffer_fadeout(
                    x->buffer.bframes - 1,
                    b,
                    x->buffer.nchans,
                    static_cast<long>(ease_pos),
                    -direction,
                    x->fade.globalramp);
            }
            x->fade.recordfade = 0;
        }
        x->fade.recfadeflag = 0;
        x->timing.recordhead = -1;
    }
}

/**
 * @brief Handle forward direction boundary wrapping for jump mode
 *
 * When jumpflag is set and moving forward, wraps the playhead when
 * it exceeds loop boundaries.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified in-place)
 * @param direction Playback direction
 */
inline void process_forward_jump_boundary(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction) noexcept
{
    if (*accuratehead > x->loop.maxloop) {
        *accuratehead = *accuratehead - (x->loop.maxloop - x->loop.minloop);
        process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    } else if (*accuratehead < 0.0) {
        *accuratehead = x->loop.maxloop + *accuratehead;
        process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    }
}

/**
 * @brief Handle reverse direction boundary wrapping for jump mode
 *
 * When jumpflag is set and moving in reverse, wraps the playhead when
 * it exceeds buffer boundaries.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified in-place)
 * @param direction Playback direction
 */
inline void process_reverse_jump_boundary(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction) noexcept
{
    const long setloopsize = x->loop.maxloop - x->loop.minloop;

    if (*accuratehead > (x->buffer.bframes - 1)) {
        *accuratehead = ((x->buffer.bframes - 1) - setloopsize) +
                       (*accuratehead - (x->buffer.bframes - 1));
        process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    } else if (*accuratehead < ((x->buffer.bframes - 1) - x->loop.maxloop)) {
        *accuratehead = (x->buffer.bframes - 1) -
                       (((x->buffer.bframes - 1) - setloopsize) - *accuratehead);
        process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
    }
}

/**
 * @brief Handle forward direction boundaries with wrap mode
 *
 * When wrapflag is set and moving forward, wraps the playhead within
 * the loop region.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified in-place)
 * @param direction Playback direction
 */
inline void process_forward_wrap_boundary(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction) noexcept
{
    const long setloopsize = x->loop.maxloop - x->loop.minloop;

    if (*accuratehead > x->loop.maxloop) {
        *accuratehead = *accuratehead - setloopsize;
        process_recording_cleanup(x, b, *accuratehead, direction, 0, x->loop.maxloop);
    } else if (*accuratehead < 0.0) {
        *accuratehead = x->loop.maxloop + setloopsize;
        process_recording_cleanup(x, b, *accuratehead, direction, 0, x->loop.minloop);
    }
}

/**
 * @brief Handle reverse direction boundaries with wrap mode
 *
 * When wrapflag is set and moving in reverse, wraps the playhead within
 * the loop region.
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified in-place)
 * @param direction Playback direction
 */
inline void process_reverse_wrap_boundary(
    t_karma* x,
    float* b,
    double* accuratehead,
    char direction) noexcept
{
    const long setloopsize = x->loop.maxloop - x->loop.minloop;

    if (*accuratehead < ((x->buffer.bframes - 1) - x->loop.maxloop)) {
        *accuratehead = (x->buffer.bframes - 1) -
                       (((x->buffer.bframes - 1) - setloopsize) - *accuratehead);
        process_recording_cleanup(
            x, b, *accuratehead, direction, 0,
            (x->buffer.bframes - 1) - x->loop.maxloop);
    } else if (*accuratehead > (x->buffer.bframes - 1)) {
        *accuratehead = ((x->buffer.bframes - 1) - setloopsize) +
                       (*accuratehead - (x->buffer.bframes - 1));
        process_recording_cleanup(
            x, b, *accuratehead, direction, 0,
            x->buffer.bframes - 1);
    }
}

/**
 * @brief Main loop boundary processing function
 *
 * Advances playhead by speed and handles all boundary conditions:
 * - Jump mode: wraps to opposite end of loop
 * - Wrap mode: wraps within loop region
 * - Normal mode: stops at loop boundaries
 *
 * @param x Karma object
 * @param b Buffer pointer
 * @param accuratehead Playhead position (modified in-place)
 * @param speed Playback speed
 * @param direction Playback direction
 * @param setloopsize Size of loop in samples
 * @param wrapflag True to wrap within loop, false to stop at boundaries
 * @param jumpflag True to jump/wrap to opposite end
 */
inline void process_loop_boundary(
    t_karma* x,
    float* b,
    double* accuratehead,
    double speed,
    char direction,
    long setloopsize,
    t_bool wrapflag,
    t_bool jumpflag) noexcept
{
    double speedsrscaled = speed * x->timing.srscale;

    // Limit speed during recording to prevent instability
    if (x->state.record) {
        const double speed_limit = setloopsize / static_cast<double>(KARMA_SPEED_LIMIT_DIVISOR);
        if (fabs(speedsrscaled) > speed_limit) {
            speedsrscaled = speed_limit * direction;
        }
    }

    *accuratehead = *accuratehead + speedsrscaled;

    if (jumpflag) {
        // Handle boundary wrapping for forward/reverse directions
        if (x->state.directionorig >= 0) {
            process_forward_jump_boundary(x, b, accuratehead, direction);
        } else {
            process_reverse_jump_boundary(x, b, accuratehead, direction);
        }
    } else {
        // Regular window/position constraints handling
        if (wrapflag) {
            // Check if playhead is outside the loop window
            if ((*accuratehead > x->loop.endloop) && (*accuratehead < x->loop.startloop)) {
                *accuratehead = (direction >= 0) ? x->loop.startloop : x->loop.endloop;
                process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
            } else if (x->state.directionorig >= 0) {
                process_forward_wrap_boundary(x, b, accuratehead, direction);
            } else {
                process_reverse_wrap_boundary(x, b, accuratehead, direction);
            }
        } else {
            // Not wrapflag - stop at loop boundaries
            if ((*accuratehead > x->loop.endloop) || (*accuratehead < x->loop.startloop)) {
                *accuratehead = (direction >= 0) ? x->loop.startloop : x->loop.endloop;
                process_recording_cleanup(x, b, *accuratehead, direction, 1, 0);
            }
        }
    }
}

} // namespace karma

#endif // KARMA_LOOP_BOUNDS_HPP
