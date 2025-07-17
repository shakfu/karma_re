# karma analysis

This project analyzes the mono version of Rodrigo Constanzo's & raja's & pete's [karma~1.6](https://github.com/rconstanzo/karma) and  will try to make some changes to the code to reduce its complexity and hopefully make it more understandable.

There will probably be some experiements with some AI code-editors to try to refactor some of the more complex functions.

Initial analysis of complexity (via gnu-complexity) of only mono perform version is:

```text
NOTE: proc ease_bufoff in file source/projects/karma_tilde/karma~.c line 170
	nesting depth reached level 6
NOTE: proc ease_bufon in file source/projects/karma_tilde/karma~.c line 203
	nesting depth reached level 6
NOTE: proc karma_buf_change_internal in file source/projects/karma_tilde/karma~.c line 720
	nesting depth reached level 7
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_setloop_internal in file source/projects/karma_tilde/karma~.c line 957
	nesting depth reached level 5
NOTE: proc karma_record in file source/projects/karma_tilde/karma~.c line 1391
	nesting depth reached level 9
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_mono_perform in file source/projects/karma_tilde/karma~.c line 1620
	nesting depth reached level 11
==>	*seriously consider rewriting the procedure*.
Complexity Scores
Score | ln-ct | nc-lns| file-name(line): proc-name
   54      85      76   source/projects/karma_tilde/karma~.c(1391): karma_record
   94     185     134   source/projects/karma_tilde/karma~.c(720): karma_buf_change_internal
 3397    1067     975   source/projects/karma_tilde/karma~.c(1620): karma_mono_perform
total nc-lns     1185
```

The complexity score of 3397 is literally off the charts (`I only wish I were kidding.` territory).


## Changes so far

- [x] analyzed with `clang-tidy` and commented out not active parts of code.

- [x] removed stereo and quad perform functions

- [x] converted to .cpp temporarily to benefit from lambda functions during refactoring

- [x] refactor some of the smaller functions if possible

- [x] added `control_state` and `human_state` enums to make state changes clearer.


