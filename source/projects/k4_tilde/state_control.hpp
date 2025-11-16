#ifndef KARMA_STATE_CONTROL_HPP
#define KARMA_STATE_CONTROL_HPP

#include "state_machine.hpp"

// =============================================================================
// KARMA STATE CONTROL - Control State Machine Processing
// =============================================================================
// The main control state machine that processes state transitions triggered by
// user messages (play, stop, record, append, jump). This 11-state switch
// statement coordinates fade flags, recording flags, and playback flags to
// ensure smooth transitions between different operational modes.

namespace karma {

/**
 * @brief Process control state machine transitions
 *
 * Main control state machine that handles all state transitions triggered by
 * user messages. This implements an 11-state switch statement that coordinates:
 * - Record enable/disable with appropriate fades
 * - Play start/stop with ramp management
 * - Jump positioning with crossfades
 * - Append mode for extending loops
 * - Loop determination and initialization
 *
 * The state machine sets various fade flags (recfadeflag, playfadeflag) which
 * are then processed during the DSP loop to apply smooth transitions.
 *
 * States handled:
 * - ZERO: No-op state
 * - RECORD_INITIAL_LOOP: Start initial loop recording
 * - RECORD_ALT: Alternate recording mode with specific fade behavior
 * - RECORD_OFF: Turn off recording with fadeout
 * - PLAY_ALT: Alternate play mode with specific fade behavior
 * - PLAY_ON: Start playback
 * - STOP_ALT: Stop with alternate fade behavior
 * - STOP_REGULAR: Regular stop with appropriate fades
 * - JUMP: Jump to position with crossfade
 * - APPEND: Append to existing loop
 * - APPEND_SPECIAL: Special append mode with alternate flag
 * - RECORD_ON: Turn on recording with fadein
 *
 * @param x Karma object (for accessing alternateflag and snrfade)
 * @param statecontrol Control state (modified - typically reset to ZERO after processing)
 * @param record Recording active flag (modified based on state)
 * @param go Playback active flag (modified based on state)
 * @param triginit Trigger initialization flag (modified based on state)
 * @param loopdetermine Loop determination flag (modified based on state)
 * @param recordfade Recording fade counter (modified - typically reset to 0)
 * @param recfadeflag Recording fade flag (modified - various values for different behaviors)
 * @param playfade Playback fade counter (modified - typically reset to 0)
 * @param playfadeflag Playback fade flag (modified - various values for different behaviors)
 * @param recendmark Recording end marker (modified - used to mark loop boundaries)
 */
inline void process_state_control(
    t_karma* x,
    control_state_t* statecontrol,
    t_bool* record,
    t_bool* go,
    t_bool* triginit,
    t_bool* loopdetermine,
    long* recordfade,
    char* recfadeflag,
    long* playfade,
    char* playfadeflag,
    char* recendmark) noexcept
{
    t_bool* alternateflag = &x->state.alternateflag;
    double* snrfade = &x->fade.snrfade;

    switch (*statecontrol) // "all-in-one 'switch' statement to catch and handle
                           // all(most) messages" - raja
    {
    case control_state_t::ZERO:
        break;
    case control_state_t::RECORD_INITIAL_LOOP:
        *record = *go = *triginit = *loopdetermine = 1;
        *statecontrol = control_state_t::ZERO;
        *recordfade = *recfadeflag = *playfade = *playfadeflag = 0;
        break;
    case control_state_t::RECORD_ALT:
        *recendmark = 3;
        *record = *recfadeflag = *playfadeflag = 1;
        *statecontrol = control_state_t::ZERO;
        *playfade = *recordfade = 0;
        break;
    case control_state_t::RECORD_OFF:
        *recfadeflag = 1;
        *playfadeflag = 3;
        *statecontrol = control_state_t::ZERO;
        *playfade = *recordfade = 0;
        break;
    case control_state_t::PLAY_ALT:
        *recendmark = 2;
        *recfadeflag = *playfadeflag = 1;
        *statecontrol = control_state_t::ZERO;
        *playfade = *recordfade = 0;
        break;
    case control_state_t::PLAY_ON:
        *triginit = 1; // ?!?!
        *statecontrol = control_state_t::ZERO;
        break;
    case control_state_t::STOP_ALT:
        *playfade = *recordfade = 0;
        *recendmark = *playfadeflag = *recfadeflag = 1;
        *statecontrol = control_state_t::ZERO;
        break;
    case control_state_t::STOP_REGULAR:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 1;
        }
        *playfade = 0;
        *playfadeflag = 1;
        *statecontrol = control_state_t::ZERO;
        break;
    case control_state_t::JUMP:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 2;
        }
        *playfade = 0;
        *playfadeflag = 2;
        *statecontrol = control_state_t::ZERO;
        break;
    case control_state_t::APPEND:
        *playfadeflag = 4; // !! modified in perform loop switch case(s) for
                           // playing behind append
        *playfade = 0;
        *statecontrol = control_state_t::ZERO;
        break;
    case control_state_t::APPEND_SPECIAL:
        *record = *loopdetermine = *alternateflag = 1;
        *snrfade = 0.0;
        *statecontrol = control_state_t::ZERO;
        *recordfade = *recfadeflag = 0;
        break;
    case control_state_t::RECORD_ON:
        *playfadeflag = 3;
        *recfadeflag = 5;
        *statecontrol = control_state_t::ZERO;
        *recordfade = *playfade = 0;
        break;
    }
}

} // namespace karma

#endif // KARMA_STATE_CONTROL_HPP
