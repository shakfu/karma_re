# karma analysis

This project is a variant of [karma~1.6](https://github.com/rconstanzo/karma) which tries to make some changes to make the code more understandable.

I will try to use some AI editors to try to refactor some of the more complex functions.

Analysis of gnu-complexity of the abbreviated (only mono perform) version is:

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