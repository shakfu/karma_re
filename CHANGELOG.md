# Changelog


## [0.1.x]



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