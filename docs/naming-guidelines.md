# Naming Guidelines

1. **Public functions** that should have `karma_` prefix and be in the header
2. **Private functions** that should have `kh_` prefix and NOT be in the header
3. **General naming improvements** based on best practices


## Private Functions

```c
static inline double kh_linear_interp(...)
static inline double kh_cubic_interp(...)
static inline double kh_spline_interp(...)
static inline double kh_ease_record(...)
static inline double kh_ease_switchramp(...)
static inline void kh_ease_bufoff(...)
static inline void kh_apply_fade(...)
static inline void kh_ease_bufon(...)

static inline void kh_process_recording_fade_completion(...)
static inline void kh_calculate_sync_output(...)
static inline void kh_apply_ipoke_interpolation(...)
static inline void kh_init_buffer_properties(...)
static inline void kh_process_recording_cleanup(...)
static inline void kh_process_forward_jump_boundary(...)
static inline void kh_process_reverse_jump_boundary(...)
static inline void kh_process_forward_wrap_boundary(...)
static inline void kh_process_reverse_wrap_boundary(...)
static inline void kh_process_loop_boundary(...)
static inline double kh_perform_playback_interpolation(...)
static inline void kh_process_playfade_state(...)
static inline void kh_process_loop_initialization(...)
static inline void kh_process_initial_loop_creation(...)
static inline long kh_wrap_index(...)
static inline void kh_interp_index(...)
static inline void kh_setloop_internal(...)
static inline void kh_process_state_control(...)
static inline void kh_initialize_perform_vars(...)
static inline void kh_process_direction_change(...)
static inline void kh_process_record_toggle(...)
static inline t_bool kh_validate_buffer(...)
static inline void kh_parse_loop_points_sym(...)
static inline void kh_parse_numeric_arg(...)
static inline void kh_process_argc_args(...)
static inline void kh_process_ipoke_recording(...)
static inline void kh_process_recording_fade(...)
static inline void kh_process_jump_logic(...)
static inline void kh_process_initial_loop_ipoke_recording(...)
static inline void kh_process_initial_loop_boundary_constraints(...)
static inline double kh_process_audio_interpolation(...)
```