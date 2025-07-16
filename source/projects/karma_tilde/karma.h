/**
    @file
    karma~.c
 
    @ingroup
    msp
 
    @name
    karma~
    
    @realname
    karma~
 
    @type
    object
    
    @module
    karma
 
    @author
    raja (original to version 1.4) & pete (version 1.5 and 1.6)
    
    @digest
    Varispeed Audio Looper
    
    @description
    <o>karma~</o> is a dynamically lengthed varispeed record/playback looper object with complex functionality
 
    @discussion
    Rodrigo is crazy
 
    @category
    karma, looper, varispeed, msp, audio, external
 
    @keywords
    loop, looping, looper, varispeed, playback, buffer
 
    @seealso
    buffer~, groove~, record~, poke~, ipoke~
    
    @owner
    Rodrigo Constanzo
 __________________________________________________________________
 */

/*
karma~ version 1.6 by pete, basically karma~ version 1.4 by raja...
...with some bug fixes and code refactoring
November 2017
N.B. - [at]-commenting for 'DoctorMax' auto documentation

TODO version 1.6:
fix: a bunch of bugs & stuff, incl. ...
- perform loop in/out updating relating to inability to call multiple methods to the object ...
... by using comma-separated message boxes (etc) <<-- big bug, probably unfixable until version 2 rewrite
- when switching buffer whilst recording, new buffer does not behave like 'inital loop' on 'stop' / 'play'

TODO version 2.0:
rewrite from scratch, take multiple perform routines out and put
interpolation routines and ipoke out into seperate files
then will be able to integrate 'rubberband' and add better ipoke interpolations etc
also look into raja's new 'crossfade' ideas as an optional alternative to 'switch & ramp'
and possibly do seperate externals for different elements (e.g. karmaplay~, karmapoke~, karmaphase~, ...)
*/

#include "stdlib.h"
#include "math.h"

#include "ext.h"            // max
#include "ext_obex.h"       // attributes

#include "ext_buffer.h"     // buffer~
#include "z_dsp.h"          // msp

#include "ext_atomic.h"


typedef struct t_karma t_karma;

t_max_err   karma_syncout_set(t_karma *x, t_object *attr, long argc, t_atom *argv);

void       *karma_new(t_symbol *s, short argc, t_atom *argv);
void        karma_free(t_karma *x);

void        karma_float(t_karma *x, double speedfloat);
void        karma_stop(t_karma *x);
void        karma_play(t_karma *x);
void        karma_record(t_karma *x);
//void      karma_select_internal(t_karma *x, double selectionstart, double selectionlength);
void        karma_select_start(t_karma *x, double positionstart);

t_max_err   karma_buf_notify(t_karma *x, t_symbol *s, t_symbol *msg, void *sndr, void *dat);

void        karma_assist(t_karma *x, void *b, long m, long a, char *s);
void        karma_buf_dblclick(t_karma *x);

void        karma_overdub(t_karma *x, double amplitude);
void        karma_select_size(t_karma *x, double duration);
//void      karma_setloop_internal(t_karma *x, t_symbol *s, short argc, t_atom *argv);
void        karma_setloop(t_karma *x, t_symbol *s, short ac, t_atom *av);
void        karma_resetloop(t_karma *x);
//void      karma_loop_multiply(t_karma *x, double multiplier); // <<-- TODO

void        karma_buf_setup(t_karma *x, t_symbol *s);
void        karma_buf_modify(t_karma *x, t_buffer_obj *b);
void        karma_clock_list(t_karma *x);
//void      karma_buf_values_internal(t_karma *x, double low, double high, long loop_points_flag, t_bool caller);
//void      karma_buf_change_internal(t_karma *x, t_symbol *s, short argc, t_atom *argv);
void        karma_buf_change(t_karma *x, t_symbol *s, short ac, t_atom *av);
//void      karma_offset(t_karma *x, long channeloffset);   // <<-- TODO

void        karma_jump(t_karma *x, double jumpposition);
void        karma_append(t_karma *x);

void karma_dsp64(t_karma *x, t_object *dsp64, short *count, double srate, long vecount, long flags);
void karma_quad_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_stereo_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr);
void karma_mono_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr);

