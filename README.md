# karma_re~

As I have used Rodrigo Constanzo's & raja's & pete's amazing [karma~1.6](https://github.com/rconstanzo/karma) Max/MSP audio looper external in [another Max project](https://github.com/shakfu/groovin), I was curious enough about how it worked that I tried to read the c code of the external. 

I personally found the code to be very complex and difficult to understand, so I started to try to make it more understandable for me by doing the following:

- Drop the stereo and quad perform functions and just focus on refactoring the mono perform function

- Extract smaller functions from complex functions

- Add meaningful enums to make things more understandable

- Use `clang-format`, `clang-tidy`, and other AI tools to help in the refactoring process

- Use complexity tools like `gnu-complexity` to target the most complex parts of the code

- Use code analysis tools like `cflow` to figure out the overall call graph.


## Status

So far the project has produced three variants (which may not have the full featureset of `karma~` 1.6):

1. A refactored mono version which seemingly works ok, 
2. An experimental stereo version based on (1)
3. An experimental `mc.` variant based on (1)

Here are some graphs to illustrate the changes:


### karma~

```sh
% cloc source/projects/karma_re_tilde/karma_re\~.c
       1 text file.
       1 unique file.
       0 files ignored.

github.com/AlDanial/cloc v 2.06  T=0.01 s (78.6 files/s, 333280.5 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                                1            414            337           3489
-------------------------------------------------------------------------------

% make complexity
NOTE: proc ease_bufoff in file source/projects/karma_tilde/karma~.c line 265
	nesting depth reached level 6
NOTE: proc ease_bufon in file source/projects/karma_tilde/karma~.c line 298
	nesting depth reached level 6
NOTE: proc karma_buf_change_internal in file source/projects/karma_tilde/karma~.c line 920
	nesting depth reached level 7
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_setloop_internal in file source/projects/karma_tilde/karma~.c line 1157
	nesting depth reached level 5
NOTE: proc karma_record in file source/projects/karma_tilde/karma~.c line 1591
	nesting depth reached level 9
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_mono_perform in file source/projects/karma_tilde/karma~.c line 1833
	nesting depth reached level 11
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_stereo_perform in file source/projects/karma_tilde/karma~.c line 2905
	nesting depth reached level 12
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_quad_perform in file source/projects/karma_tilde/karma~.c line 4907
	nesting depth reached level 12
==>	*seriously consider rewriting the procedure*.
Complexity Scores
Score | ln-ct | nc-lns| file-name(line): proc-name
   54      85      77   source/projects/karma_tilde/karma~.c(1591): karma_record
   94     185     134   source/projects/karma_tilde/karma~.c(920): karma_buf_change_internal
 3407    1066     985   source/projects/karma_tilde/karma~.c(1833): karma_mono_perform
14048    1996    1867   source/projects/karma_tilde/karma~.c(2905): karma_stereo_perform
32778    4093    3859   source/projects/karma_tilde/karma~.c(4907): karma_quad_perform
total nc-lns     6922
```

[pdf call-graph](./docs/cflow/karma_cflow_filter0.pdf)

![original call-graph](./docs/cflow/karma_cflow_filter0.svg)


### karma_re~

```sh
% cloc source/projects/karma_tilde/karma\~.c
       1 text file.
       1 unique file.
       0 files ignored.

github.com/AlDanial/cloc v 2.06  T=0.02 s (45.9 files/s, 413706.5 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                                1            515            595           7903
-------------------------------------------------------------------------------

% make complexity-re
NOTE: proc kh_process_loop_initialization in file source/projects/karma_re_tilde/karma_re~.c line 644
	nesting depth reached level 5
NOTE: proc kh_process_initial_loop_creation in file source/projects/karma_re_tilde/karma_re~.c line 702
	nesting depth reached level 6
NOTE: proc kh_setloop_internal in file source/projects/karma_re_tilde/karma_re~.c line 1343
	nesting depth reached level 5
NOTE: proc karma_dsp64 in file source/projects/karma_re_tilde/karma_re~.c line 1992
	nesting depth reached level 6
NOTE: proc karma_mono_perform in file source/projects/karma_re_tilde/karma_re~.c line 2179
	nesting depth reached level 9
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_stereo_perform in file source/projects/karma_re_tilde/karma_re~.c line 2625
	nesting depth reached level 9
==>	*seriously consider rewriting the procedure*.
NOTE: proc karma_poly_perform in file source/projects/karma_re_tilde/karma_re~.c line 3082
	nesting depth reached level 8
==>	*seriously consider rewriting the procedure*.
NOTE: proc kh_process_argc_args in file source/projects/karma_re_tilde/karma_re~.c line 3542
	nesting depth reached level 5
NOTE: proc kh_process_recording_fade in file source/projects/karma_re_tilde/karma_re~.c line 3694
	nesting depth reached level 5
NOTE: proc kh_process_initial_loop_ipoke_recording in file source/projects/karma_re_tilde/karma_re~.c line 3754
	nesting depth reached level 7
==>	*seriously consider rewriting the procedure*.
NOTE: proc kh_process_initial_loop_boundary_constraints in file source/projects/karma_re_tilde/karma_re~.c line 3873
	nesting depth reached level 5
NOTE: proc kh_process_ipoke_recording_stereo in file source/projects/karma_re_tilde/karma_re~.c line 3989
	nesting depth reached level 5
NOTE: proc kh_process_initial_loop_ipoke_recording_stereo in file source/projects/karma_re_tilde/karma_re~.c line 4048
	nesting depth reached level 8
==>	*seriously consider rewriting the procedure*.
Complexity Scores
Score | ln-ct | nc-lns| file-name(line): proc-name
   30     139     118   source/projects/karma_re_tilde/karma_re~.c(1343): kh_setloop_internal
  150     114     111   source/projects/karma_re_tilde/karma_re~.c(3754): kh_process_initial_loop_ipoke_recording
  186     379     328   source/projects/karma_re_tilde/karma_re~.c(3082): karma_poly_perform
  334     190     187   source/projects/karma_re_tilde/karma_re~.c(4048): kh_process_initial_loop_ipoke_recording_stereo
  341     440     344   source/projects/karma_re_tilde/karma_re~.c(2179): karma_mono_perform
  378     451     393   source/projects/karma_re_tilde/karma_re~.c(2625): karma_stereo_perform
total nc-lns     1481
```

[pdf call-graph](./docs/cflow/karma_re_cflow_filter0.pdf)

![original call-graph](./docs/cflow/karma_re_cflow_filter0.svg)






