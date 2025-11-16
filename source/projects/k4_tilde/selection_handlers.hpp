#ifndef KARMA_SELECTION_HANDLERS_HPP
#define KARMA_SELECTION_HANDLERS_HPP

#include "dsp_utils.hpp"
#include "loop_config.hpp"

// =============================================================================
// KARMA SELECTION HANDLERS - Selection/Windowing and Status Reporting
// =============================================================================
// Functions for managing selection windowing (position/size), loop reset, and
// status reporting via clock outlet.

namespace karma {

/**
 * @brief Reset loop to initial boundaries
 *
 * Restores loop points to the values set at initialization or buffer setup.
 * Uses the stored initiallow/initialhigh values from the karma object.
 *
 * @param x Karma object (modified - updates loop boundaries)
 */
inline void reset_loop_boundaries(t_karma* x) noexcept
{
    long   points_flag = 1;    // initial low/high points stored as samples internally
    t_bool callerid = false;   // false = called from "resetloop"
    double initiallow = static_cast<double>(x->loop.initiallow);
    double initialhigh = static_cast<double>(x->loop.initialhigh);

    process_buf_values_internal(x, initiallow, initialhigh, points_flag, callerid);
}

/**
 * @brief Output status data list to outlet
 *
 * Sends playback status information via the clock outlet when reportlist > 0.
 * Reschedules itself at reportlist interval (milliseconds) for continuous updates.
 *
 * Output list format (7 elements):
 * - [0] position: normalized position 0.0-1.0 within loop
 * - [1] go: play flag (int, 1=playing)
 * - [2] record: record/overdub flag (int)
 * - [3] start: loop start time (float, milliseconds)
 * - [4] end: loop end time (float, milliseconds)
 * - [5] window: selection window size (float, milliseconds)
 * - [6] state: human_state_t enum value (int)
 *
 * @param x Karma object (uses messout outlet, tclock for rescheduling)
 */
inline void output_status_list(t_karma* x) noexcept
{
    t_bool rlgtz = x->reportlist > 0;

    if (rlgtz) // ('reportlist 0' == off, else milliseconds)
    {
        long frames = x->buffer.bframes - 1; // !! no '- 1' ??
        long maxloop = x->loop.maxloop;
        long minloop = x->loop.minloop;
        long setloopsize;

        double bmsr = x->buffer.bmsr;
        double playhead = x->timing.playhead;
        double selection = x->timing.selection;
        double normalisedposition;
        setloopsize = maxloop - minloop;

        float reversestart = (static_cast<double>(frames - setloopsize));
        float forwardstart = (static_cast<double>(minloop)); // ??           // (minloop + 1)
        float reverseend = (static_cast<double>(frames));
        float forwardend = (static_cast<double>(maxloop)); // !!           // (maxloop + 1)        // !! only
                             // broken on initial buffersize report ?? !!
        float selectionsize = (selection * (static_cast<double>(setloopsize))); // (setloopsize
                                                                   // + 1)    //
                                                                   // !! only
                                                                   // broken on
                                                                   // initial
                                                                   // buffersize
                                                                   // report ??
                                                                   // !!

        t_bool directflag = x->state.directionorig < 0; // !! reverse = 1, forward = 0
        t_bool record = x->state.record; // pointless (and actually is 'record' or
                                         // 'overdub')
        t_bool go = x->state.go;         // pointless (and actually this is on whenever
                                         // transport is,...
                                         // ...not stricly just 'play')
        human_state_t statehuman = x->state.statehuman;
        //  ((playhead-(frames-maxloop))/setloopsize) :
        //  ((playhead-startloop)/setloopsize)  // ??
        normalisedposition = CLAMP(
            directflag ? ((playhead - (frames - setloopsize)) / setloopsize)
                       : ((playhead - minloop) / setloopsize),
            0., 1.);

        t_atom datalist[7];                              // !! reverse logics are wrong ??
        atom_setfloat(datalist + 0, normalisedposition); // position float normalised 0..1
        atom_setlong(datalist + 1, go);                  // play flag int
        atom_setlong(datalist + 2, record);              // record flag int
        atom_setfloat(
            datalist + 3,
            (directflag ? reversestart : forwardstart) / bmsr); // start float ms
        atom_setfloat(
            datalist + 4,
            (directflag ? reverseend : forwardend) / bmsr);  // end float ms
        atom_setfloat(datalist + 5, (selectionsize / bmsr)); // window float ms
        atom_setlong(datalist + 6, static_cast<long>(statehuman));  // state flag int

        outlet_list(x->messout, 0L, 7, datalist);
        //      outlet_list(x->messout, gensym("list"), 7, datalist);

        if (sys_getdspstate() && (rlgtz)) { // '&& (x->reportlist > 0)' ??
            clock_delay(x->tclock, x->reportlist);
        }
    }
}

/**
 * @brief Set selection window start position
 *
 * Sets the starting position of the selection window within the loop.
 * Handles wrap-around logic when selection extends beyond loop boundaries.
 * Adjusts loop.startloop and loop.endloop based on direction and position.
 *
 * Position is normalized 0.0-1.0 (phase within the loop range).
 * Behavior differs based on original recording direction (forward/reverse).
 *
 * @param x Karma object (modified - updates startloop, endloop, wrapflag)
 * @param positionstart Normalized position 0.0-1.0
 */
inline void set_selection_start(t_karma* x, double positionstart) noexcept
{
    long bfrmaesminusone, setloopsize;
    x->timing.selstart = CLAMP(positionstart, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->state.loopdetermine) {
        setloopsize = x->loop.maxloop - x->loop.minloop;

        if (x->state.directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->buffer.bframes - 1;

            x->loop.startloop = CLAMP(
                (bfrmaesminusone - x->loop.maxloop) + (positionstart * setloopsize),
                bfrmaesminusone - x->loop.maxloop, bfrmaesminusone);
            x->loop.endloop = x->loop.startloop + (x->timing.selection * x->loop.maxloop);

            if (x->loop.endloop > bfrmaesminusone) {
                x->loop.endloop = (bfrmaesminusone - setloopsize)
                    + (x->loop.endloop - bfrmaesminusone);
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            x->loop.startloop = CLAMP(
                ((positionstart * setloopsize) + x->loop.minloop), x->loop.minloop,
                x->loop.maxloop); // no need for CLAMP ??
            x->loop.endloop = x->loop.startloop + (x->timing.selection * setloopsize);

            if (x->loop.endloop > x->loop.maxloop) {
                x->loop.endloop = x->loop.endloop - setloopsize;
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

/**
 * @brief Set selection window size
 *
 * Sets the duration/size of the selection window within the loop.
 * Handles wrap-around logic when selection extends beyond loop boundaries.
 * Adjusts loop.endloop based on startloop and selection size.
 *
 * Duration is normalized 0.0-1.0 (phase of the loop range).
 * Behavior differs based on original recording direction (forward/reverse).
 *
 * @param x Karma object (modified - updates selection, endloop, wrapflag)
 * @param duration Normalized duration 0.0-1.0
 */
inline void set_selection_size(t_karma* x, double duration) noexcept
{
    long bfrmaesminusone, setloopsize;

    // double minsampsnorm = x->timing.bvsnorm * 0.5;           // half vectorsize
    // samples minimum as normalised value  // !! buffer sr !! x->timing.selection =
    // (duration < 0.0) ? 0.0 : duration; // !! allow zero for rodrigo !!
    x->timing.selection = CLAMP(duration, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->state.loopdetermine) {
        setloopsize = x->loop.maxloop - x->loop.minloop;
        x->loop.endloop = x->loop.startloop + (x->timing.selection * setloopsize);

        if (x->state.directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->buffer.bframes - 1;

            if (x->loop.endloop > bfrmaesminusone) {
                x->loop.endloop = (bfrmaesminusone - setloopsize)
                    + (x->loop.endloop - bfrmaesminusone);
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            if (x->loop.endloop > x->loop.maxloop) {
                x->loop.endloop = x->loop.endloop - setloopsize;
                x->state.wrapflag = 1;
            } else {
                x->state.wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

} // namespace karma

#endif // KARMA_SELECTION_HANDLERS_HPP
