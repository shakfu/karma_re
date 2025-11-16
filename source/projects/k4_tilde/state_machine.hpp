#ifndef KARMA_STATE_MACHINE_HPP
#define KARMA_STATE_MACHINE_HPP

// =============================================================================
// KARMA STATE MACHINE - Type-Safe State Transitions
// =============================================================================
// Utilities for managing the dual state machine (control + human states).

namespace karma {

/**
 * @brief Map control state to human-facing state
 *
 * Converts the detailed internal control state to a simplified
 * user-facing state for UI feedback.
 *
 * @param control Internal DSP control state
 * @return User-facing human state
 */
inline constexpr human_state_t control_to_human_state(control_state_t control) noexcept {
    switch (control) {
        case control_state_t::ZERO:
            return human_state_t::STOP;

        case control_state_t::RECORD_INITIAL_LOOP:
        case control_state_t::RECORD_ON:
            return human_state_t::RECORD;

        case control_state_t::RECORD_ALT:
            return human_state_t::OVERDUB;

        case control_state_t::PLAY_ON:
        case control_state_t::PLAY_ALT:
        case control_state_t::JUMP:
            return human_state_t::PLAY;

        case control_state_t::APPEND:
        case control_state_t::APPEND_SPECIAL:
            return human_state_t::APPEND;

        case control_state_t::STOP_ALT:
        case control_state_t::STOP_REGULAR:
        case control_state_t::RECORD_OFF:
            return human_state_t::STOP;

        default:
            return human_state_t::STOP;
    }
}

/**
 * @brief Check if state represents active recording
 */
inline constexpr bool is_recording_state(control_state_t state) noexcept {
    return state == control_state_t::RECORD_INITIAL_LOOP ||
           state == control_state_t::RECORD_ON ||
           state == control_state_t::RECORD_ALT;
}

/**
 * @brief Check if state represents active playback
 */
inline constexpr bool is_playing_state(control_state_t state) noexcept {
    return state == control_state_t::PLAY_ON ||
           state == control_state_t::PLAY_ALT ||
           state == control_state_t::JUMP;
}

/**
 * @brief Check if state represents stopping/stopped
 */
inline constexpr bool is_stopped_state(control_state_t state) noexcept {
    return state == control_state_t::ZERO ||
           state == control_state_t::STOP_ALT ||
           state == control_state_t::STOP_REGULAR ||
           state == control_state_t::RECORD_OFF;
}

/**
 * @brief Check if state involves overdubbing
 */
inline constexpr bool is_overdub_state(control_state_t state) noexcept {
    return state == control_state_t::RECORD_ALT ||
           state == control_state_t::PLAY_ALT;
}

/**
 * @brief Check if state involves append mode
 */
inline constexpr bool is_append_state(control_state_t state) noexcept {
    return state == control_state_t::APPEND ||
           state == control_state_t::APPEND_SPECIAL;
}

/**
 * @brief Get human-readable state name for debugging
 */
inline const char* state_name(control_state_t state) noexcept {
    switch (state) {
        case control_state_t::ZERO:                return "ZERO";
        case control_state_t::RECORD_INITIAL_LOOP: return "RECORD_INITIAL_LOOP";
        case control_state_t::RECORD_ALT:          return "RECORD_ALT";
        case control_state_t::RECORD_OFF:          return "RECORD_OFF";
        case control_state_t::PLAY_ALT:            return "PLAY_ALT";
        case control_state_t::PLAY_ON:             return "PLAY_ON";
        case control_state_t::STOP_ALT:            return "STOP_ALT";
        case control_state_t::STOP_REGULAR:        return "STOP_REGULAR";
        case control_state_t::JUMP:                return "JUMP";
        case control_state_t::APPEND:              return "APPEND";
        case control_state_t::APPEND_SPECIAL:      return "APPEND_SPECIAL";
        case control_state_t::RECORD_ON:           return "RECORD_ON";
        default:                                    return "UNKNOWN";
    }
}

/**
 * @brief Get human-readable state name for human states
 */
inline const char* state_name(human_state_t state) noexcept {
    switch (state) {
        case human_state_t::STOP:    return "STOP";
        case human_state_t::PLAY:    return "PLAY";
        case human_state_t::RECORD:  return "RECORD";
        case human_state_t::OVERDUB: return "OVERDUB";
        case human_state_t::APPEND:  return "APPEND";
        case human_state_t::INITIAL: return "INITIAL";
        default:                      return "UNKNOWN";
    }
}

} // namespace karma

#endif // KARMA_STATE_MACHINE_HPP
