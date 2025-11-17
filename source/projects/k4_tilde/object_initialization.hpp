#ifndef KARMA_OBJECT_INITIALIZATION_HPP
#define KARMA_OBJECT_INITIALIZATION_HPP

#include "poly_arrays.hpp"

// =============================================================================
// KARMA OBJECT INITIALIZATION - Object Construction Helpers
// =============================================================================
// Functions for initializing karma~ objects during instantiation. Handles
// argument parsing, DSP setup, memory allocation, state initialization, and
// outlet/clock creation.

namespace karma {

/**
 * @brief Parse instantiation arguments
 *
 * Extracts buffer name and channel count from Max object arguments.
 * Validates argument count and types.
 *
 * Arguments:
 * - argv[0]: buffer name (required, symbol)
 * - argv[1]: channel count (optional, int) - default 1
 * - argv[2+]: ignored with warning
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param bufname Output buffer name (set to nullptr if no args)
 * @param chans Output channel count (set to 0 if not specified)
 * @param attrstart Output attribute start index
 * @param x Karma object (for error reporting)
 */
inline void parse_instantiation_args(
    short argc,
    t_atom* argv,
    t_symbol** bufname,
    long* chans,
    long* attrstart,
    t_karma* x) noexcept
{
    *bufname = nullptr;
    *chans = 0;
    *attrstart = attr_args_offset(argc, argv);

    // should do better argument checks here
    if (*attrstart && argv) {
        *bufname = atom_getsym(argv + 0);
        // @arg 0 @name buffer_name @optional 0 @type symbol @digest Name of
        // <o>buffer~</o> to be associated with the <o>karma~</o> instance
        // @description Essential argument: <o>karma~</o> will not operate
        // without an associated <o>buffer~</o> <br /> The associated
        // <o>buffer~</o> determines memory and length (if associating a buffer~
        // of <b>0 ms</b> in size <o>karma~</o> will do nothing) <br /> The
        // associated <o>buffer~</o> can be changed on the fly (see the
        // <m>set</m> message) but one must be present on instantiation <br />
        if (*attrstart > 1) {
            *chans = atom_getlong(argv + 1);
            if (*attrstart > 2) {
                // object_error((t_object *)x, "rodrigo! third arg no longer
                // used! use new @syncout attribute instead!");
                object_warn(
                    reinterpret_cast<t_object*>(x),
                    "too many arguments to karma~, ignoring additional crap");
            }
        }
    /*  } else {
            object_error((t_object *)x, "karma~ will not load without an
       associated buffer~ declaration"); goto zero;
    */  }
}

/**
 * @brief Setup DSP inlets based on channel count
 *
 * Creates appropriate number of signal inlets:
 * - Mono (chans <= 1): 2 inlets (audio + speed)
 * - Stereo (chans == 2): 3 inlets (audio L, audio R, speed)
 * - Multichannel (chans >= 4): 5 inlets (4 audio + speed)
 *
 * Normalizes channel count to 1, 2, or 4.
 *
 * @param x Karma object (modified - DSP inlets created)
 * @param chans Channel count (modified - normalized to 1/2/4)
 */
inline void setup_dsp_inlets(t_karma* x, long* chans) noexcept
{
    if (*chans <= 1) {
        // one audio channel inlet, one signal speed inlet
        dsp_setup(reinterpret_cast<t_pxobject*>(x), 2);
        *chans = 1;
    } else if (*chans == 2) {
        // two audio channel inlets, one signal speed inlet
        dsp_setup(reinterpret_cast<t_pxobject*>(x), 3);
        *chans = 2;
    } else {
        // four audio channel inlets, one signal speed inlet
        dsp_setup(reinterpret_cast<t_pxobject*>(x), 5);
        *chans = 4;
    }
}

/**
 * @brief Allocate multichannel processing arrays
 *
 * Allocates PolyArrays for channels beyond the first 4. Calculates appropriate
 * allocation size based on requested channels and configured limits. Warns if
 * channel count exceeds KARMA_ABSOLUTE_CHANNEL_LIMIT.
 *
 * @param x Karma object (modified - poly_arrays allocated)
 * @param chans Requested channel count
 * @return true if allocation succeeded, false on failure
 */
inline t_bool allocate_poly_arrays(t_karma* x, long chans) noexcept
{
    // Allocate multichannel processing arrays for maximum expected channels
    // Calculate channel allocation count (clamp to limits)
    long requested_chans = chans;
    long poly_maxchans = (chans > KARMA_STRUCT_CHANNEL_COUNT) ?
                         ((chans > KARMA_ABSOLUTE_CHANNEL_LIMIT) ? KARMA_ABSOLUTE_CHANNEL_LIMIT : chans) :
                         KARMA_POLY_PREALLOC_COUNT;

    // Warn if we had to clamp the channel count
    if (chans > KARMA_ABSOLUTE_CHANNEL_LIMIT) {
        object_warn(reinterpret_cast<t_object*>(x), "Requested %ld channels, but maximum configured is %d. Using %d channels.",
                   requested_chans, KARMA_ABSOLUTE_CHANNEL_LIMIT, KARMA_ABSOLUTE_CHANNEL_LIMIT);
    }

    // Allocate multichannel processing arrays using RAII wrapper
    x->poly_arrays = new (std::nothrow) karma::PolyArrays(poly_maxchans);
    if (!x->poly_arrays || !x->poly_arrays->is_valid()) {
        object_error(reinterpret_cast<t_object*>(x), "Failed to allocate memory for multichannel processing arrays");
        delete x->poly_arrays;
        x->poly_arrays = nullptr;
        return false;
    }

    x->input_channels = chans;  // Initialize input channel count
    return true;
}

/**
 * @brief Initialize karma object state to defaults
 *
 * Sets all state variables, timing parameters, audio parameters, fade settings,
 * and loop boundaries to their initial values. Called during object construction.
 *
 * @param x Karma object (modified - all state initialized)
 */
inline void initialize_object_state(t_karma* x) noexcept
{
    x->timing.recordhead = -1;
    x->reportlist = KARMA_DEFAULT_REPORT_TIME_MS;                          // ms
    x->fade.snrramp = x->fade.globalramp = KARMA_DEFAULT_FADE_SAMPLES;  // samps...
    x->fade.playfade = x->fade.recordfade = KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE; // ...
    x->timing.ssr = sys_getsr();
    x->timing.vs = sys_getblksize();
    x->timing.vsnorm = x->timing.vs / x->timing.ssr;

    x->audio.overdubprev = 1.0;
    x->audio.overdubamp = 1.0;
    x->speedfloat = 1.0;
    x->islooped = 1;

    x->fade.snrtype = switchramp_type_t::SINE_IN;
    x->audio.interpflag = interp_type_t::CUBIC;
    x->fade.playfadeflag = 0;
    x->fade.recfadeflag = 0;
    x->state.recordinit = 0;
    x->state.initinit = 0;
    x->state.append = 0;
    x->state.jumpflag = 0;
    x->state.statecontrol = control_state_t::ZERO;
    x->state.statehuman = human_state_t::STOP;
    x->state.stopallowed = 0;
    x->state.go = 0;
    x->state.triginit = 0;
    x->state.directionprev = 0;
    x->state.directionorig = 0;
    x->state.recordprev = 0;
    x->state.record = 0;
    x->state.alternateflag = 0;
    x->state.recendmark = 0;
    x->audio.pokesteps = 0;
    x->state.wrapflag = 0;
    x->state.loopdetermine = 0;
    x->audio.writeval1 = x->audio.writeval2 = x->audio.writeval3 = x->audio.writeval4 = 0;
    x->timing.maxhead = 0.0;
    x->timing.playhead = 0.0;
    x->loop.initiallow = -1;
    x->loop.initialhigh = -1;
    x->timing.selstart = 0.0;
    x->timing.jumphead = 0.0;
    x->fade.snrfade = 0.0;
    x->audio.o1dif = x->audio.o2dif = x->audio.o3dif = x->audio.o4dif = 0.0;
    x->audio.o1prev = x->audio.o2prev = x->audio.o3prev = x->audio.o4prev = 0.0;
}

/**
 * @brief Create outlets and clock
 *
 * Creates message outlet and clock for status reporting. Validates creation
 * success and cleans up on failure.
 *
 * @param x Karma object (modified - messout and tclock created)
 * @return true if creation succeeded, false on failure
 */
inline t_bool create_outlets_and_clock(t_karma* x) noexcept
{
    x->messout = listout(x); // data
    if (!x->messout) {
        object_error(reinterpret_cast<t_object*>(x), "Failed to create list outlet");
        return false;
    }

    x->tclock = clock_new(reinterpret_cast<t_object*>(x), reinterpret_cast<method>(karma_clock_list));
    if (!x->tclock) {
        object_error(reinterpret_cast<t_object*>(x), "Failed to create clock");
        object_free(x->messout);
        return false;
    }

    return true;
}

/**
 * @brief Create signal outlets based on configuration
 *
 * Creates appropriate signal outlets based on channel count and syncoutlet setting:
 * - Mono: 1 audio outlet (+ optional sync)
 * - Stereo: 2 audio outlets (+ optional sync)
 * - Multichannel: 1 multichannel outlet (+ optional sync)
 *
 * Sync outlet is created last (rightmost) when enabled.
 *
 * @param x Karma object (modified - signal outlets created)
 * @param chans Channel count (1, 2, or 4+)
 * @param syncoutlet Sync outlet flag (0=disabled, 1=enabled)
 */
inline void create_signal_outlets(t_karma* x, long chans, long syncoutlet) noexcept
{
    if (chans <= 1) { // mono
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "signal");     // first: audio output
    } else if (chans == 2) {         // stereo
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "signal");     // second: audio output 2
        outlet_new(x, "signal");     // first: audio output 1
    } else {                         // multichannel (4+)
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "multichannelsignal"); // multichannel audio output
    }
}

/**
 * @brief Finalize object setup
 *
 * Sets final object flags and properties:
 * - initskip flag (enables DSP processing)
 * - Z_NO_INPLACE flag (prevents in-place processing)
 * - Z_MC_INLETS flag (enables multichannel inlet support)
 *
 * @param x Karma object (modified - flags set)
 */
inline void finalize_object_setup(t_karma* x) noexcept
{
    x->state.initskip = 1;
    x->k_ob.z_misc |= Z_NO_INPLACE;

    // Enable multichannel inlet support for all channel counts
    // This allows the object to receive both single and multichannel patch cords
    x->k_ob.z_misc |= Z_MC_INLETS;
}

} // namespace karma

#endif // KARMA_OBJECT_INITIALIZATION_HPP
