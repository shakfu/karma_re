# Changelog


## [0.1.x]


## [0.1.3]

- Added comprehensive constants to `karma.h`, replacing 15+ hardcoded values across the codebase, improving readability and maintainability.

```c
// Constants for magic numbers
#define KARMA_MIN_LOOP_SIZE 4096
#define KARMA_DEFAULT_FADE_SAMPLES 256
#define KARMA_DEFAULT_FADE_SAMPLES_PLUS_ONE 257
#define KARMA_SPEED_LIMIT_DIVISOR 1024
#define KARMA_SENTINEL_VALUE -999.0
#define KARMA_DEFAULT_REPORT_TIME_MS 50
#define KARMA_MAX_RAMP_SAMPLES 2048
#define KARMA_ASSIST_STRING_MAX_LEN 256
```

- Additional refactoring.

## [0.1.2]

- Fixed Uninitialized variable `setloopsize` (lines 2203, 2651, 3129):
    - Added initialization to 0 in all three function declarations
    - This prevents the "11th function call argument is an uninitialized value" warnings

- Fixed Pointer type mismatch for jumpflag (lines 245, 248, 2416, 2425, 2868, 2877, 3411):
    - Changed function declarations `from char* jumpflag to t_bool* jumpflag`
    - Updated both function signatures and implementations to use consistent types
    - This fixes the passing `t_bool *` to parameter of type `char *` warnings

- Fixed Array bounds violation (lines 3175, 3419):
    - Fixed improper array access on struct members `o1prev`, `o2prev`, etc.
    - Replaced unsafe` (&x->audio.o1prev)[i]` array indexing with explicit conditional assignments
    - Used individual struct member access to avoid treating single double fields as arrays

- Added `mc.` (multichannel) version of perform

- Added stereo version of perform

## [0.1.1]

- Fixed play-reset bug

- Refactored mono version of karma~

- Refactored easing functions