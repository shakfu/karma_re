// karma_core.c -- the Max-free karma looper DSP. Hand-owned source.
//
// Originally extracted verbatim from the reference karma~.c (see gen_core.sh.orig
// for the historical generator), now refactored directly. Held sample-for-sample
// to the reference by the offline harness in tests/ (make check); refactor freely
// as long as that stays green.
#include "karma_core.h"
#include "karma_state.h"   // named states for the control/perform state machine
#include "karma_interp.h"  // buffer-read interpolation kernels (linear/cubic/spline + interp_index)
#include "karma_ipoke.h"   // ease/ipoke write kernels (record fades, buffer declick)

// buffer-modify notification is a host concern; no-op in the core.
static void karma_buf_modify(t_karma *x, void *b) { (void)x; (void)b; }

// ---- control methods (verbatim) ----
void karma_float(t_karma *x, double speedfloat)
{
    long inlet = proxy_getinlet((t_object *)x);
    long chans = (long)x->ochans;

    if (inlet == chans) {   // if speed inlet
        x->speedfloat = speedfloat;
    }
}

void karma_select_start(t_karma *x, double positionstart)   // positionstart = "position" float message
{
    int64_t bfrmaesminusone, setloopsize;
    x->selstart = CLAMP(positionstart, 0., 1.);
    
    // for dealing with selection-out-of-bounds logic:
    
    if (!x->loopdetermine)
    {
        setloopsize = x->maxloop - x->minloop;
        
        if (x->directionorig < 0)   // if originally in reverse
        {
            bfrmaesminusone = x->bframes - 1;

            x->startloop = CLAMP( (bfrmaesminusone - x->maxloop) + (positionstart * setloopsize), bfrmaesminusone - x->maxloop, bfrmaesminusone );
            x->endloop = x->startloop + (x->selection * x->maxloop);
            
            if (x->endloop > bfrmaesminusone) {
                x->endloop = (bfrmaesminusone - setloopsize) + (x->endloop - bfrmaesminusone);
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0;    // selection-in-bounds
            }
            
        } else {                    // if originally forwards

            x->startloop = CLAMP( ((positionstart * setloopsize) + x->minloop), x->minloop, x->maxloop );   // no need for CLAMP ??
            x->endloop = x->startloop + (x->selection * setloopsize);
            
            if (x->endloop > x->maxloop) {
                x->endloop = x->endloop - setloopsize;
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0;    // selection-in-bounds
            }
            
        }
    }
}

void karma_select_size(t_karma *x, double duration)     // duration = "window" float message
{
    int64_t bfrmaesminusone, setloopsize;
    
    //double minsampsnorm = x->bvsnorm * 0.5;           // half vectorsize samples minimum as normalised value  // !! buffer sr !!
    //x->selection = (duration < 0.0) ? 0.0 : duration; // !! allow zero for rodrigo !!
    x->selection = CLAMP(duration, 0., 1.);
    
    // for dealing with selection-out-of-bounds logic:
    
    if (!x->loopdetermine)
    {
        setloopsize = x->maxloop - x->minloop;
        x->endloop = x->startloop + (x->selection * setloopsize);
        
        if (x->directionorig < 0)   // if originally in reverse
        {
            bfrmaesminusone = x->bframes - 1;
            
            if (x->endloop > bfrmaesminusone) {
                x->endloop = (bfrmaesminusone - setloopsize) + (x->endloop - bfrmaesminusone);
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0;    // selection-in-bounds
            }
            
        } else {                    // if originally forwards
            
            if(x->endloop > x->maxloop) {
                x->endloop = x->endloop - setloopsize;
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0;    // selection-in-bounds
            }
            
        }
    }
}

void karma_stop(t_karma *x)
{
    if (x->initinit) {
        if (x->stopallowed) {
            x->statecontrol = x->alternateflag ? SC_STOP_ALT : SC_STOP;
            x->append = 0;
            x->statehuman = SH_STOP;
            x->stopallowed = 0;
        }
    }
}

void karma_play(t_karma *x)
{
    if ((!x->go) && (x->append)) {
        x->statecontrol = SC_APPEND;
        x->snrfade = 0.0;   // !! should disable ??
    } else if ((x->record) || (x->append)) {
        x->statecontrol = x->alternateflag ? SC_PLAY_ALT : SC_REC_OFF;
    } else {
        x->statecontrol = SC_PLAY_ON;
    }

    x->go = 1;
    x->statehuman = SH_PLAY;
    x->stopallowed = 1;
}

void karma_record(t_karma *x)
{
    float *b;
    long i;
    char sc, sh;
    t_bool record, go, altflag, append, init;
    int64_t bframes, rchans;  // !! local 'rchans' = 'nchans' not 'bchans' !!
    
    t_buffer_obj *buf = x->bufio.ctx;

    record = x->record;
    go = x->go;
    altflag = x->alternateflag;
    append = x->append;
    init = x->recordinit;
    sc = x->statecontrol;
    sh = x->statehuman;
    
    x->stopallowed = 1;
    
    if (record) {
        if (altflag) {
            sc = SC_REC_ALT;
            sh = SH_OVERDUB;            // ?? is this wrong?, it is not neccessarily overdub ??
        } else {
            sc = SC_REC_OFF;
            sh = (sh == SH_OVERDUB) ? SH_PLAY : SH_REC_EXISTING; // !! hack !! (but works !!)
        }
    } else {
        if (append) {
            if (go) {
                if (altflag) {
                    sc = SC_REC_ALT;
                    sh = SH_OVERDUB; // ?? is this wrong?, it is not neccessarily overdub ??
                } else {
                    sc = SC_APPEND_SPECIAL;// !!
                    sh = SH_APPEND;
                }
            } else {
                sc = SC_REC_INITIAL;
                sh = SH_REC_INITIAL;
            }
        } else {
            if (!go) {
                init = 1;
                if (buf) {
                    rchans = x->bchans;     // !! nchans not bchans = only record onto channel(s) currently used by karma~...
                    bframes = x->bframes;   // ...(leave other channels in tact)    <<-- BOLLOX
                    b = ((float*)x->bufio.lock(x->bufio.ctx));
                    if (!b)
                        goto zero;
                    
                    for (i = 0; i < bframes; i++) {
                        if (rchans > 1) {
                            b[i * rchans] = 0.0;
                            b[(i * rchans) + 1] = 0.0;
                            if (rchans > 2) {
                                b[(i * rchans) + 2] = 0.0;
                                if (rchans > 3) {
                                    b[(i * rchans) + 3] = 0.0;
                                }
                            }
                        } else {
                            b[i] = 0.0;
                        }
                    }
                    
                    x->bufio.set_dirty(x->bufio.ctx);
                    x->bufio.unlock(x->bufio.ctx);
                }
                sc = SC_REC_INITIAL;
                sh = SH_REC_INITIAL;
            } else {            // !! not 'record', not 'append', is 'go' ??
                sc = SC_REC_ON;        // !! ?? seems wrong
                sh = SH_OVERDUB;       // !! is this overdub ?? seems wrong
            }
        }
    }
    
    go = 1;
    x->go = go;
    x->recordinit = init;
    x->statecontrol = sc;
    x->statehuman = sh;
    
zero:
    return;
}

void karma_append(t_karma *x)
{
    if (x->recordinit) {
        if ((!x->append) && (!x->loopdetermine)) {
            x->append = 1;
            x->maxloop = (x->bframes - 1);
            x->statecontrol = SC_APPEND;
            x->statehuman = SH_APPEND;
            x->stopallowed = 1;
        } else {
            object_error((t_object *)x, "can't append if already appending, or during 'initial-loop', or if buffer~ is full");
        }
    } else {
        object_error((t_object *)x, "warning! no 'append' registered until at least one loop has been created first");
    }
}

void karma_overdub(t_karma *x, double amplitude)
{
    x->overdubamp = CLAMP(amplitude, 0.0, 1.0);
}

void karma_jump(t_karma *x, double jumpposition)
{
    if (x->initinit) {
        if ( !((x->loopdetermine)&&(!x->record)) ) {
            x->statecontrol = SC_JUMP;
            x->jumphead = CLAMP(jumpposition, 0., 1.);  // for now phase only, TODO - ms & samples
//          x->statehuman = 1;                          // no - 'jump' is whatever 'statehuman' currently is (most likely 'play')
            x->stopallowed = 1;
        }
    }
}

// ---- perform (one channel-generic routine) ----
//
// The reference shipped three near-identical perform routines (mono/stereo/quad),
// each an unrolled copy of the same logic per channel and, internally, branched
// again on the buffer channel count. They are unified here into a single routine
// that loops over the channels: head movement, loop-window constraints, fades and
// the state machine run once per sample (channel-invariant), while the per-channel
// read / interpolation / ipoke-write are inner loops over nproc = min(pchans,
// ochans) channels. Output is ochans-wide; any output channel beyond the buffer's
// channel count is silenced -- exactly as the reference's pchans<ochans branches
// did. Verified sample-for-sample against the reference across every scenario
// (incl. the pchans<ochans cases) by the offline harness.
//
// The three public entry points below forward to it (preserving the API/ABI the
// host shell calls), so nothing outside this file changes.
static void karma_perform(t_karma *x, double **ins, double **outs, long vcount)
{
    long    syncoutlet  = x->syncoutlet;
    long    ochans      = (long)x->ochans;

    double *in[4], *out[4];
    long    ch;
    for (ch = 0; ch < ochans; ch++) { in[ch] = ins[ch]; out[ch] = outs[ch]; }
    double *inspeed = ins[ochans];              // speed (if signal connected)
    double *outPh   = syncoutlet ? outs[ochans] : 0;   // sync (if @syncout 1)

    long    n = vcount;
    short   speedinlet  = x->speedconnect;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif, pokesteps;
    double speed, speedfloat, overdubamp, overdubprev, ovdbdif, selstart, selection;
    double frac, snrfade, globalramp, snrramp;
    double osamp[4], recin[4], writeval[4], coeff[4], oprev[4], odif[4];
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, dirt, wrapflag, triginit;
    char direction, directionprev, directionorig, statecontrol, playfadeflag, recfadeflag, recendmark;
    int64_t playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans, snrtype, interp, nproc;
    int64_t frames, startloop, endloop, playhead, recordhead, minloop, maxloop, setloopsize;
    int64_t initiallow, initialhigh;

    t_buffer_obj *buf = x->bufio.ctx;
    float *b = ((float*)x->bufio.lock(x->bufio.ctx));

    record          = x->record;
    recordprev      = x->recordprev;
    dirt            = 0;
    if (!b)
        goto zero;
    if (record || recordprev)
        dirt        = 1;
    if (x->buf_modified) {
        karma_buf_modify(x, buf);
        x->buf_modified  = false;
    }

    oprev[0] = x->o1prev; oprev[1] = x->o2prev; oprev[2] = x->o3prev; oprev[3] = x->o4prev;
    odif[0]  = x->o1dif;  odif[1]  = x->o2dif;  odif[2]  = x->o3dif;  odif[3]  = x->o4dif;
    writeval[0] = x->writeval1; writeval[1] = x->writeval2; writeval[2] = x->writeval3; writeval[3] = x->writeval4;

    go              = x->go;
    statecontrol    = x->statecontrol;
    playfadeflag    = x->playfadeflag;
    recfadeflag     = x->recfadeflag;
    recordhead      = x->recordhead;
    alternateflag   = x->alternateflag;
    pchans          = x->bchans;
    srscale         = x->srscale;
    frames          = x->bframes;
    triginit        = x->triginit;
    jumpflag        = x->jumpflag;
    append          = x->append;
    directionorig   = x->directionorig;
    directionprev   = x->directionprev;
    minloop         = x->minloop;
    maxloop         = x->maxloop;
    initiallow      = x->initiallow;
    initialhigh     = x->initialhigh;
    selection       = x->selection;
    loopdetermine   = x->loopdetermine;
    startloop       = x->startloop;
    selstart        = x->selstart;
    endloop         = x->endloop;
    recendmark      = x->recendmark;
    overdubamp      = x->overdubprev;
    overdubprev     = x->overdubamp;
    ovdbdif         = (overdubamp != overdubprev) ? ((overdubprev - overdubamp) / n) : 0.0;
    recordfade      = x->recordfade;
    playfade        = x->playfade;
    accuratehead    = x->playhead;
    playhead        = trunc(accuratehead);
    maxhead         = x->maxhead;
    wrapflag        = x->wrapflag;
    jumphead        = x->jumphead;
    pokesteps       = x->pokesteps;
    snrfade         = x->snrfade;
    globalramp      = (double)x->globalramp;
    snrramp         = (double)x->snrramp;
    snrtype         = x->snrtype;
    interp          = x->interpflag;
    speedfloat      = x->speedfloat;

    nproc           = (pchans < ochans) ? pchans : ochans;  // channels actually read/recorded

    switch (statecontrol)   // "all-in-one 'switch' statement to catch and handle all(most) messages" - raja
    {
        case SC_ZERO:               // no pending message
            break;
        case SC_REC_INITIAL:        // record initial loop
            record = go = triginit = loopdetermine = 1;
            recordfade = recfadeflag = playfade = playfadeflag = statecontrol = 0;
            break;
        case SC_REC_ALT:            // record alternateflag (wtf is 'alternateflag' ('rectoo') ?!)  // in to OVERDUB ??
            recendmark = RECEND_ENTER_OVERDUB;
            record = recfadeflag = playfadeflag = 1;
            playfade = recordfade = statecontrol = 0;
            break;
        case SC_REC_OFF:            // record off regular
            recfadeflag = REC_FADE_OUT;
            playfadeflag = PLAY_FADE_RECOFF;
            playfade = recordfade = statecontrol = 0;
            break;
        case SC_PLAY_ALT:           // play alternateflag (wtf is 'alternateflag' ('rectoo') ?!)    // out of OVERDUB ??
            recendmark = RECEND_EXIT_OVERDUB;
            recfadeflag = playfadeflag = 1;
            playfade = recordfade = statecontrol = 0;
            break;
        case SC_PLAY_ON:            // play on regular
            triginit = 1;   // ?!?!
            statecontrol = 0;
            break;
        case SC_STOP_ALT:           // stop alternateflag (wtf is 'alternateflag' ('rectoo') ?!)    // after OVERDUB ??
            playfade = recordfade = 0;
            recendmark = playfadeflag = recfadeflag = 1;
            statecontrol = 0;
            break;
        case SC_STOP:               // stop regular
            if (record) {
                recordfade = 0;
                recfadeflag = REC_FADE_OUT;
            }
            playfade = 0;
            playfadeflag = PLAY_FADE_OUT;
            statecontrol = 0;
            break;
        case SC_JUMP:               // jump
            if (record) {
                recordfade = 0;
                recfadeflag = REC_FADE_JUMP;
            }
            playfade = 0;
            playfadeflag = PLAY_FADE_JUMP;
            statecontrol = 0;
            break;
        case SC_APPEND:             // append
            playfadeflag = PLAY_FADE_APPEND;   // !! modified in perform loop switch case(s) for playing behind append
            playfade = 0;
            statecontrol = 0;
            break;
        case SC_APPEND_SPECIAL:     // special case append (what is special about it ?!)   // in to RECORD / OVERDUB ??
            record = loopdetermine = alternateflag = 1;
            snrfade = 0.0;
            recordfade = recfadeflag = statecontrol = 0;
            break;
        case SC_REC_ON:             // record on regular (when ?! not looped ?!)
            playfadeflag = PLAY_FADE_RECOFF;
            recfadeflag = REC_FADE_IN;
            recordfade = playfade = statecontrol = 0;
            break;          // !!
    }

    //  raja notes:
    // 'snrfade = 0.0' triggers switch&ramp (declick play)
    // 'recordhead = -1' triggers ipoke-interp cuts and accompanies buf~ fades (declick record)

    while (n--)
    {
        for (ch = 0; ch < nproc; ch++)
            recin[ch] = *in[ch]++;
        speed = speedinlet ? *inspeed++ : speedfloat;   // signal of float ?
        direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

        // declick for change of 'dir'ection
        if (directionprev != direction) {
            if (record && globalramp) {
                ease_bufoff(frames - 1, b, pchans, recordhead, -direction, globalramp);
                recordfade = recfadeflag = 0;
                recordhead = -1;
            }
            snrfade = 0.0;
        }

        if ((record - recordprev) < 0) {           // samp @record-off
            if (globalramp)
                ease_bufoff(frames - 1, b, pchans, recordhead, direction, globalramp);
            //initialhigh = loopdetermine ? recordhead : initialhigh;
            recordhead = -1;
            dirt = 1;
        } else if ((record - recordprev) > 0) {    // samp @record-on
            recordfade = recfadeflag = 0;
            if (speed < 1.0)
                snrfade = 0.0;
            if (globalramp)
                ease_bufoff(frames - 1, b, pchans, accuratehead, -direction, globalramp);
        }
        recordprev = record;

        if (!loopdetermine)
        {
            if (go)
            {
                // --- head movement (the reference's inlined "calculate_head") ---
                if (triginit)
                {
                    if (recendmark)  // calculate end of loop
                    {
                        if (directionorig >= 0)
                        {
                            maxloop = CLAMP(maxhead, 4096, frames - 1); // why 4096 ??
                            setloopsize = maxloop - minloop;
                            accuratehead = startloop = minloop + (selstart * setloopsize);
                            endloop = startloop + (selection * setloopsize);
                            if (endloop > maxloop) {
                                endloop = endloop - (setloopsize + 1);
                                wrapflag = 1;
                            } else {
                                wrapflag = 0;
                            }
                            if (direction < 0) {
                                if (globalramp)
                                    ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                            }
                        } else {
                            maxloop = CLAMP((frames - 1) - maxhead, 4096, frames - 1);
                            setloopsize = maxloop - minloop;    // ((frames - 1) - setloopsize - minloop)   // ??
                            startloop = ((frames - 1) - setloopsize) + (selstart * setloopsize);    // ((frames - 1) - maxloop) + (selstart * maxloop);   // ??
                            accuratehead = endloop = startloop + (selection * setloopsize);         // startloop + (selection * maxloop);   ??
                            if (endloop > (frames - 1)) {
                                endloop = ((frames - 1) - setloopsize) + (endloop - frames);
                                wrapflag = 1;
                            } else {
                                wrapflag = 0;
                            }
                            accuratehead = endloop;
                            if (direction > 0) {
                                if (globalramp)
                                    ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                            }
                        }
                        if (globalramp)
                            ease_bufoff(frames - 1, b, pchans, maxhead, -direction, globalramp);
                        recordhead = -1;
                        snrfade = 0.0;
                        triginit = 0;
                        append = alternateflag = recendmark = 0;
                    } else {    // jump / play (inside 'window')
                        setloopsize = maxloop - minloop;
                        if (jumpflag)
                            accuratehead = (directionorig >= 0) ? ((jumphead * setloopsize) + minloop) : (((frames - 1) - maxloop) + (jumphead * setloopsize));
                        else
                            accuratehead = (direction < 0) ? endloop : startloop;
                        if (record) {
                            if (globalramp) {
                                ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                recordfade = 0;
                            }
                            recordhead = -1;
                            recfadeflag = 0;
                        }
                        snrfade = 0.0;
                        triginit = 0;
                    }
                } else {        // jump-based constraints (outside 'window')
                    setloopsize = maxloop - minloop;
                    speedsrscaled = speed * srscale;

                    if (record)
                        speedsrscaled = (fabs(speedsrscaled) > (setloopsize / 1024)) ? ((setloopsize / 1024) * direction) : speedsrscaled;
                    accuratehead = accuratehead + speedsrscaled;

                    if (jumpflag)
                    {
                        if (wrapflag) {
                            if ((accuratehead < endloop) || (accuratehead > startloop))
                                jumpflag = 0;
                        } else {
                            if ((accuratehead < endloop) && (accuratehead > startloop))
                                jumpflag = 0;
                        }
                        if (directionorig >= 0)
                        {
                            if (accuratehead > maxloop)
                            {
                                accuratehead = accuratehead - setloopsize;
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            } else if (accuratehead < 0.0) {
                                accuratehead = maxloop + accuratehead;
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            }
                        } else {
                            if (accuratehead > (frames - 1))
                            {
                                accuratehead = ((frames - 1) - setloopsize) + (accuratehead - (frames - 1));    // ...((frames - 1) - maxloop)...   // ??
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            } else if (accuratehead < ((frames - 1) - maxloop)) {
                                accuratehead = (frames - 1) - (((frames - 1) - setloopsize) - accuratehead);    // ...((frames - 1) - maxloop)... // ??
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            }
                        }
                    } else {    // regular 'window' / 'position' constraints
                        if (wrapflag)
                        {
                            if ((accuratehead > endloop) && (accuratehead < startloop))
                            {
                                accuratehead = (direction >= 0) ? startloop : endloop;
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            } else if (directionorig >= 0) {
                                if (accuratehead > maxloop)
                                {
                                    accuratehead = accuratehead - setloopsize;  // fixed position ??
                                    snrfade = 0.0;
                                    if (record) {
                                        if (globalramp) {
                                            ease_bufoff(frames - 1, b, pchans, maxloop, -direction, globalramp);
                                            recordfade = 0;
                                        }
                                        recfadeflag = 0;
                                        recordhead = -1;
                                    }
                                }
                                else if (accuratehead < 0.0)
                                {
                                    accuratehead = maxloop + setloopsize;       // !! this is surely completely wrong ??
                                    snrfade = 0.0;
                                    if (record) {
                                        if (globalramp) {
                                            ease_bufoff(frames - 1, b, pchans, minloop, -direction, globalramp);     // 0.0  // ??
                                            recordfade = 0;
                                        }
                                        recfadeflag = 0;
                                        recordhead = -1;
                                    }
                                }
                            } else {    // reverse
                                if (accuratehead < ((frames - 1) - maxloop))
                                {
                                    accuratehead = (frames - 1) - (((frames - 1) - setloopsize) - accuratehead);    // ...- maxloop)... // ??
                                    snrfade = 0.0;
                                    if (record)
                                    {
                                        if (globalramp) {
                                            ease_bufoff(frames - 1, b, pchans, ((frames - 1) - maxloop), -direction, globalramp);
                                            recordfade = 0;
                                        }
                                        recfadeflag = 0;
                                        recordhead = -1;
                                    }
                                } else if (accuratehead > (frames - 1)) {
                                    accuratehead = ((frames - 1) - setloopsize) + (accuratehead - (frames - 1));    // ...- maxloop)...   // ??
                                    snrfade = 0.0;
                                    if (record) {
                                        if (globalramp) {
                                            ease_bufoff(frames - 1, b, pchans, (frames - 1), -direction, globalramp);
                                            recordfade = 0;
                                        }
                                        recfadeflag = 0;
                                        recordhead = -1;
                                    }
                                }
                            }
                        } else {    // (not wrapflag)
                            if ((accuratehead > endloop) || (accuratehead < startloop))
                            {
                                accuratehead = (direction >= 0) ? startloop : endloop;
                                snrfade = 0.0;
                                if (record) {
                                    if (globalramp) {
                                        ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                        recordfade = 0;
                                    }
                                    recfadeflag = 0;
                                    recordhead = -1;
                                }
                            }
                        }
                    }
                }
                // --- end head movement ---

                // interp ratio
                playhead = trunc(accuratehead);
                if (direction > 0) {
                    frac = accuratehead - playhead;
                } else if (direction < 0) {
                    frac = 1.0 - (accuratehead - playhead);
                } else {
                    frac = 0.0;
                }                                                                                   // setloopsize  // ??
                interp_index(playhead, &interp0, &interp1, &interp2, &interp3, direction, directionorig, maxloop, frames - 1);  // samp-indices

                for (ch = 0; ch < nproc; ch++) {
                    if (record) {           // if recording do linear-interp else...
                        osamp[ch] = LINEAR_INTERP(frac, b[interp1 * pchans + ch], b[interp2 * pchans + ch]);
                    } else {                // ...cubic / spline if interpflag > 0 (default cubic)
                        if (interp == 1)
                            osamp[ch] = CUBIC_INTERP(frac, b[interp0 * pchans + ch], b[interp1 * pchans + ch], b[interp2 * pchans + ch], b[interp3 * pchans + ch]);
                        else if (interp == 2)
                            osamp[ch] = SPLINE_INTERP(frac, b[interp0 * pchans + ch], b[interp1 * pchans + ch], b[interp2 * pchans + ch], b[interp3 * pchans + ch]);
                        else
                            osamp[ch] = LINEAR_INTERP(frac, b[interp1 * pchans + ch], b[interp2 * pchans + ch]);
                    }
                }

                if (globalramp)
                {                                           // "Switch and Ramp" - http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
                    if (snrfade < 1.0)
                    {
                        if (snrfade == 0.0) {
                            for (ch = 0; ch < nproc; ch++)
                                odif[ch] = oprev[ch] - osamp[ch];
                        }
                        for (ch = 0; ch < nproc; ch++)
                            osamp[ch] += ease_switchramp(odif[ch], snrfade, snrtype);// <- easing-curv options implemented by raja
                        snrfade += 1 / snrramp;
                    }                                               // "Switch and Ramp" end

                    if (playfade < globalramp)
                    {                                               // realtime ramps for play on/off
                        for (ch = 0; ch < nproc; ch++)
                            osamp[ch] = ease_record(osamp[ch], (playfadeflag > 0), globalramp, playfade);
                        playfade++;
                        if (playfade >= globalramp)
                        {
                            switch (playfadeflag)
                            {
                                case PLAY_FADE_NONE:
                                    break;
                                case PLAY_FADE_OUT:
                                    playfadeflag = go = 0;  // record alternateflag   // play alternateflag  // stop alternateflag / regular
                                    break;
                                case PLAY_FADE_JUMP:
                                    if (!record)
                                        triginit = jumpflag = 1;
//                                  break;                  // !! no break - pass jump -> recoff !!
                                case PLAY_FADE_RECOFF:      // jump // record off reg
                                    playfadeflag = playfade = 0;
                                    break;
                                case PLAY_FADE_APPEND:      // append
                                    go = triginit = loopdetermine = 1;
                                    // !! will disbling this enable play behind append ?? should this be dependent on passing previous maxloop ??
                                    snrfade = 0.0;
                                    playfade = 0;           // !!
                                    playfadeflag = 0;
                                    break;
                            }
                        }
                    }
                } else {
                    switch (playfadeflag)
                    {
                        case PLAY_FADE_NONE:
                            break;
                        case PLAY_FADE_OUT:
                            playfadeflag = go = 0;
                            break;
                        case PLAY_FADE_JUMP:
                            if (!record)
                                triginit = jumpflag = 1;
//                          break;                                  // !! no break - pass jump -> recoff !!
                        case PLAY_FADE_RECOFF:                      // jump     // record off reg
                            playfadeflag = 0;
                            break;
                        case PLAY_FADE_APPEND:                      // append
                            go = triginit = loopdetermine = 1;
                            // !! will disbling this enable play behind append ?? should this be based on passing previous maxloop ??
                            snrfade = 0.0;
                            playfade = 0;   // !!
                            playfadeflag = 0;
                            break;
                    }
                }

            } else {
                for (ch = 0; ch < nproc; ch++)
                    osamp[ch] = 0.0;
            }

            for (ch = 0; ch < ochans; ch++) {
                double s = (ch < nproc) ? osamp[ch] : 0.0;
                oprev[ch] = s;
                *out[ch]++ = s;
            }
            if (syncoutlet) {
                setloopsize = maxloop-minloop;
                *outPh++    = (directionorig>=0) ? ((accuratehead-minloop)/setloopsize) : ((accuratehead-(frames-setloopsize))/setloopsize);
            }

            /*
             ~ipoke - originally by PA Tremblay: http://www.pierrealexandretremblay.com/welcome.html
             (modded to allow for 'selection' (window) and 'selstart' (position) to change on the fly)
             raja's razor: simplest answer to everything was:
             recin1 = ease_record(recin1 + (b[playhead * pchans] * overdubamp), recfadeflag, globalramp, recordfade); ...
             ... placed at the beginning / input of ipoke~ code to apply appropriate ramps to oldbuf + newinput (everything all-at-once) ...
             ... allows ipoke~ code to work its sample-specific math / magic accurately through the ducking / ramps even at high speed
            */
            if (record)
            {
                for (ch = 0; ch < nproc; ch++) {
                    if ((recordfade < globalramp) && (globalramp > 0.0))
                        recin[ch] = ease_record(recin[ch] + (((double)b[playhead * pchans + ch]) * overdubamp), recfadeflag, globalramp, recordfade);
                    else
                        recin[ch] += ((double)b[playhead * pchans + ch]) * overdubamp;
                }

                if (recordhead < 0) {
                    recordhead = playhead;
                    pokesteps = 0.0;
                    recplaydif = 0.0;
                    for (ch = 0; ch < nproc; ch++) writeval[ch] = 0.0;
                }

                if (recordhead == playhead) {
                    for (ch = 0; ch < nproc; ch++) writeval[ch] += recin[ch];
                    pokesteps += 1.0;
                } else {
                    if (pokesteps > 1.0) {              // linear-averaging for speed < 1x
                        for (ch = 0; ch < nproc; ch++) writeval[ch] = writeval[ch] / pokesteps;
                        pokesteps = 1.0;
                    }
                    for (ch = 0; ch < nproc; ch++) b[recordhead * pchans + ch] = writeval[ch];
                    recplaydif = (double)(playhead - recordhead);
                    if (recplaydif > 0) {               // linear-interpolation for speed > 1x
                        for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                        for (i = recordhead + 1; i < playhead; i++) {
                            for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                        }
                    } else {
                        for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                        for (i = recordhead - 1; i > playhead; i--) {
                            for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                        }
                    }
                    for (ch = 0; ch < nproc; ch++) writeval[ch] = recin[ch];
                }
                recordhead = playhead;
                dirt = 1;
            }                                           // ~ipoke end

            if (globalramp)                             // realtime ramps for record on/off
            {
                if(recordfade < globalramp)
                {
                    recordfade++;
                    if ((recfadeflag) && (recordfade >= globalramp))
                    {
                        if (recfadeflag == REC_FADE_JUMP) {
                            triginit = jumpflag = 1;
                            recordfade = 0;
                        } else if (recfadeflag == REC_FADE_IN) {
                            record = 1;
                        } else {
                            record = 0;
                        }
                        recfadeflag = 0;
                    }
                }
            } else {
                if (recfadeflag) {
                    if (recfadeflag == REC_FADE_JUMP) {
                        triginit = jumpflag = 1;
                    } else if (recfadeflag == REC_FADE_IN) {
                        record = 1;
                    } else {
                        record = 0;
                    }
                    recfadeflag = 0;
                }
            }
            directionprev = direction;

        } else {                                        // initial loop creation
        // !! is 'loopdetermine' !!

            if (go)
            {
                if (triginit)
                {
                    if (jumpflag)                       // jump
                    {
                        if (directionorig >= 0) {
                            accuratehead = jumphead * maxhead;      // !! maxhead !!
                        } else {
                            accuratehead = (frames - 1) - (((frames - 1) - maxhead) * jumphead);
                        }
                        jumpflag = 0;
                        snrfade = 0.0;
                        if (record) {
                            if (globalramp) {
                                ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                recordfade = 0;
                            }
                            recfadeflag = 0;
                            recordhead = -1;
                        }
                        triginit = 0;
                    } else if (append) {                // append
                        snrfade = 0.0;
                        triginit = 0;
                        if (record)
                        {
                            accuratehead = maxhead;                 // !! maxhead !!
                            if (globalramp) {
                                ease_bufon(frames - 1, b, pchans, accuratehead, recordhead, direction, globalramp);
                                recordfade = 0;
                            }
                            alternateflag = 1;
                            recfadeflag = 0;
                            recordhead = -1;
                        } else {
                            goto apned;
                        }
                    } else {                            // trigger start of initial loop creation
                        directionorig = direction;
                        minloop = 0.0;
                        maxloop = frames - 1;
                        maxhead = accuratehead = (direction >= 0) ? minloop : maxloop;     // (direction >= 0) ? 0.0 : (frames - 1);
                        alternateflag = 1;
                        recordhead = -1;
                        snrfade = 0.0;
                        triginit = 0;
                    }
                } else {
apned:
                    setloopsize = maxloop - minloop;                // not really required here because initial loop ??
                    speedsrscaled = speed * srscale;
                    if (record)                                     // why 1024 ??
                        speedsrscaled = (fabs(speedsrscaled) > (setloopsize / 1024)) ? ((setloopsize / 1024) * direction) : speedsrscaled;
                    accuratehead = accuratehead + speedsrscaled;
                    if (direction == directionorig)                 // buffer~ boundary constraints and registry of maximum distance traversed
                    {
                        if (accuratehead > (frames - 1))
                        {
                            accuratehead = 0.0;
                            record = append;
                            if (record) {
                                if (globalramp) {
                                    ease_bufoff(frames - 1, b, pchans, (frames - 1), -direction, globalramp);   // maxloop ??
                                    recordhead = -1;
                                    recfadeflag = recordfade = 0;
                                }
                            }
                            recendmark = triginit = 1;
                            loopdetermine = alternateflag = 0;
                            maxhead = frames - 1;
                        } else if (accuratehead < 0.0) {
                            accuratehead = frames - 1;
                            record = append;
                            if (record) {
                                if (globalramp) {
                                    ease_bufoff(frames - 1, b, pchans, minloop, -direction, globalramp);     // 0.0  // ??
                                    recordhead = -1;
                                    recfadeflag = recordfade = 0;
                                }
                            }
                            recendmark = triginit = 1;
                            loopdetermine = alternateflag = 0;
                            maxhead = 0.0;
                        } else {                                    // <- track max write position
                            if ( ((directionorig >= 0) && (maxhead < accuratehead)) || ((directionorig < 0) && (maxhead > accuratehead)) ) {
                                maxhead = accuratehead;
                            }
                        }
                    } else if (direction < 0) {                     // wraparounds for reversal while creating initial-loop
                        if (accuratehead < 0.0)
                        {
                            accuratehead = maxhead + accuratehead;
                            if (globalramp) {
                                ease_bufoff(frames - 1, b, pchans, minloop, -direction, globalramp);     // 0.0  // ??
                                recordhead = -1;
                                recfadeflag = recordfade = 0;
                            }
                        }
                    } else if (direction >= 0) {
                        if (accuratehead > (frames - 1))
                        {
                            accuratehead = maxhead + (accuratehead - (frames - 1));
                            if (globalramp) {
                                ease_bufoff(frames - 1, b, pchans, (frames - 1), -direction, globalramp);   // maxloop ??
                                recordhead = -1;
                                recfadeflag = recordfade = 0;
                            }
                        }
                    }
                //initialhigh = append ? initialhigh : maxhead;   // !! !!
                }

                playhead = trunc(accuratehead);
                if (direction > 0) {                            // interp ratio
                    frac = accuratehead - playhead;
                } else if (direction < 0) {
                    frac = 1.0 - (accuratehead - playhead);
                } else {
                    frac = 0.0;
                }

                if (globalramp)
                {
                    if (playfade < globalramp)                  // realtime ramps for play on/off
                    {
                        playfade++;
                        if (playfadeflag)
                        {
                            if (playfade >= globalramp)
                            {
                                if (playfadeflag == PLAY_FADE_JUMP) {
                                    recendmark = 4;
                                    go = 1;
                                }
                                playfadeflag = 0;
                                switch (recendmark) {
                                    case 0:
                                    case 1:
                                        go = 0;
                                        break;
                                    case 2:
                                    case 3:
                                        go = 1;
                                        playfade = 0;
                                        break;
                                    case 4:
                                        recendmark = 0;
                                        break;
                                }
                            }
                        }
                    }
                } else {
                    if (playfadeflag)
                    {
                        if (playfadeflag == PLAY_FADE_JUMP) {
                            recendmark = 4;
                            go = 1;
                        }
                        playfadeflag = 0;
                        switch (recendmark) {
                            case 0:
                            case 1:
                                go = 0;
                                break;
                            case 2:
                            case 3:
                                go = 1;
                                break;
                            case 4:
                                recendmark = 0;
                                break;
                        }
                    }
                }

            }

            for (ch = 0; ch < ochans; ch++) {
                osamp[ch] = 0.0;
                oprev[ch] = osamp[ch];
                *out[ch]++ = osamp[ch];
            }
            if (syncoutlet) {
                setloopsize = maxloop-minloop;
                *outPh++    = (directionorig>=0) ? ((accuratehead-minloop)/setloopsize) : ((accuratehead-(frames-setloopsize))/setloopsize);
            }

            // ~ipoke - originally by PA Tremblay: http://www.pierrealexandretremblay.com/welcome.html
            // (modded to assume maximum distance recorded into buffer~ as the total length)
            if (record)
            {
                for (ch = 0; ch < nproc; ch++) {
                    if ((recordfade < globalramp) && (globalramp > 0.0))
                        recin[ch] = ease_record(recin[ch] + ((double)b[playhead * pchans + ch]) * overdubamp, recfadeflag, globalramp, recordfade);
                    else
                        recin[ch] += ((double)b[playhead * pchans + ch]) * overdubamp;
                }

                if (recordhead < 0) {
                    recordhead = playhead;
                    pokesteps = 0.0;
                    recplaydif = 0.0;
                    for (ch = 0; ch < nproc; ch++) writeval[ch] = 0.0;
                }

                if (recordhead == playhead) {
                    for (ch = 0; ch < nproc; ch++) writeval[ch] += recin[ch];
                    pokesteps += 1.0;
                } else {
                    if (pokesteps > 1.0) {                          // linear-averaging for speed < 1x
                        for (ch = 0; ch < nproc; ch++) writeval[ch] = writeval[ch] / pokesteps;
                        pokesteps = 1.0;
                    }
                    for (ch = 0; ch < nproc; ch++) b[recordhead * pchans + ch] = writeval[ch];
                    recplaydif = (double)(playhead - recordhead);   // linear-interp for speed > 1x
                    if (direction != directionorig)
                    {
                        if (directionorig >= 0)
                        {
                            if (recplaydif > 0)
                            {
                                if (recplaydif > (maxhead * 0.5))
                                {
                                    recplaydif -= maxhead;
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead - 1); i >= 0; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                    for (i = maxhead; i > playhead; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                } else {
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead + 1); i < playhead; i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                }
                            } else {
                                if ((-recplaydif) > (maxhead * 0.5))
                                {
                                    recplaydif += maxhead;
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead + 1); i < (maxhead + 1); i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                    for (i = 0; i < playhead; i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                } else {
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead - 1); i > playhead; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                }
                            }
                        } else {
                            if (recplaydif > 0)
                            {
                                if (recplaydif > (((frames - 1) - (maxhead)) * 0.5))
                                {
                                    recplaydif -= ((frames - 1) - (maxhead));
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead - 1); i >= maxhead; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                    for (i = (frames - 1); i > playhead; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                } else {
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead + 1); i < playhead; i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                }
                            } else {
                                if ((-recplaydif) > (((frames - 1) - (maxhead)) * 0.5))
                                {
                                    recplaydif += ((frames - 1) - (maxhead));
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead + 1); i < frames; i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                    for (i = maxhead; i < playhead; i++) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                } else {
                                    for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                                    for (i = (recordhead - 1); i > playhead; i--) {
                                        for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                                    }
                                }
                            }
                        }
                    } else {
                        if (recplaydif > 0)
                        {
                            for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                            for (i = (recordhead + 1); i < playhead; i++) {
                                for (ch = 0; ch < nproc; ch++) { writeval[ch] += coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                            }
                        } else {
                            for (ch = 0; ch < nproc; ch++) coeff[ch] = (recin[ch] - writeval[ch]) / recplaydif;
                            for (i = (recordhead - 1); i > playhead; i--) {
                                for (ch = 0; ch < nproc; ch++) { writeval[ch] -= coeff[ch]; b[i * pchans + ch] = writeval[ch]; }
                            }
                        }
                    }
                    for (ch = 0; ch < nproc; ch++) writeval[ch] = recin[ch];
                }                           // ~ipoke end
                if (globalramp)             // realtime ramps for record on/off
                {
                    if (recordfade < globalramp)
                    {
                        recordfade++;
                        if ((recfadeflag) && (recordfade >= globalramp))
                        {
                            if (recfadeflag == REC_FADE_JUMP) {
                                recendmark = 4;
                                triginit = jumpflag = 1;
                                recordfade = 0;
                            } else if (recfadeflag == REC_FADE_IN) {
                                record = 1;
                            }
                            recfadeflag = 0;
                            //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                            switch (recendmark)
                            {
                                case 0:
                                    record = 0;
                                    break;
                                case 1:
                                    if (directionorig < 0) {
                                        maxloop = (frames - 1) - maxhead;
                                    } else {
                                        maxloop = maxhead;
                                    }
//                                  break;                  // !! no break - pass 1 -> 2 !!
                                case 2:
                                    //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                    record = loopdetermine = 0;
                                    triginit = 1;
                                    break;
                                case 3:
                                    //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                    record = triginit = 1;
                                    recordfade = loopdetermine = 0;
                                    break;
                                case 4:
                                    //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                    recendmark = 0;
                                    break;
                            }
                        }
                    }
                } else {
                    if (recfadeflag)
                    {
                        if (recfadeflag == REC_FADE_JUMP) {
                            recendmark = 4;
                            triginit = jumpflag = 1;
                        } else if (recfadeflag == REC_FADE_IN) {
                            record = 1;
                        }
                        recfadeflag = 0;
                        switch (recendmark)
                        {
                            case 0:
                                record = 0;
                                break;
                            case 1:
                                if (directionorig < 0) {
                                    maxloop = (frames - 1) - maxhead;
                                } else {
                                    maxloop = maxhead;
                                }
//                              break;                      // !! no break - pass 1 -> 2 !!
                            case 2:
                                //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                record = loopdetermine = 0;
                                triginit = 1;
                                break;
                            case 3:
                                //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                record = triginit = 1;
                                loopdetermine = 0;
                                break;
                            case 4:
                                //initial_points(minloop, maxloop, &initiallow, &initialhigh);
                                recendmark = 0;
                                break;
                        }
                    }
                }               //
                recordhead = playhead;
                dirt = 1;
                //initialhigh = maxloop;
            }
            directionprev = direction;
        }
        if (ovdbdif != 0.0)
            overdubamp = overdubamp + ovdbdif;

        initialhigh = (dirt) ? maxloop : initialhigh;  // recordhead ??
    }

    if (dirt) {                 // notify other buf-related jobs of write
        x->bufio.set_dirty(x->bufio.ctx);
    }
    x->bufio.unlock(x->bufio.ctx);

    // (report-clock arming lives in the host shell, which owns the real clock;
    // the core's verbatim block here was inert no-op shims and has been removed.)

    x->o1prev = oprev[0]; x->o2prev = oprev[1]; x->o3prev = oprev[2]; x->o4prev = oprev[3];
    x->o1dif  = odif[0];  x->o2dif  = odif[1];  x->o3dif  = odif[2];  x->o4dif  = odif[3];
    x->writeval1 = writeval[0]; x->writeval2 = writeval[1]; x->writeval3 = writeval[2]; x->writeval4 = writeval[3];

    x->maxhead          = maxhead;
    x->pokesteps        = pokesteps;
    x->wrapflag         = wrapflag;
    x->snrfade          = snrfade;
    x->playhead         = accuratehead;
    x->directionorig    = directionorig;
    x->directionprev    = directionprev;
    x->recordhead       = recordhead;
    x->alternateflag    = alternateflag;
    x->recordfade       = recordfade;
    x->triginit         = triginit;
    x->jumpflag         = jumpflag;
    x->go               = go;
    x->record           = record;
    x->recordprev       = recordprev;
    x->statecontrol     = statecontrol;
    x->playfadeflag     = playfadeflag;
    x->recfadeflag      = recfadeflag;
    x->playfade         = playfade;
    x->minloop          = minloop;
    x->maxloop          = maxloop;
    x->initiallow       = initiallow;
    x->initialhigh      = initialhigh;
    x->loopdetermine    = loopdetermine;
    x->startloop        = startloop;
    x->endloop          = endloop;
    x->overdubprev      = overdubamp;
    x->recendmark       = recendmark;
    x->append           = append;

    return;

zero:
    while (n--) {
        for (ch = 0; ch < ochans; ch++)
            *out[ch]++ = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }

    return;
}

// Public entry points -- thin forwarders to the channel-generic routine above.
// (dsp64 / nins / nouts / flgs / usr are vestigial Max perform-signature params;
// the channel count comes from x->ochans.)
void karma_mono_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr)
{
    (void)dsp64; (void)nins; (void)nouts; (void)flgs; (void)usr;
    karma_perform(x, ins, outs, vcount);
}

void karma_stereo_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr)
{
    (void)dsp64; (void)nins; (void)nouts; (void)flgs; (void)usr;
    karma_perform(x, ins, outs, vcount);
}

void karma_quad_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr)
{
    (void)dsp64; (void)nins; (void)nouts; (void)flgs; (void)usr;
    karma_perform(x, ins, outs, vcount);
}

// ---- init / configure (mirrors karma_new defaults + karma_buf_setup) ----
void karma_core_init(t_karma *x, long ochans, double ssr, double vs)
{
    memset(x, 0, sizeof(*x));
    x->recordhead = -1;
    x->snrramp = x->globalramp = 256;
    x->playfade = x->recordfade = 257;
    x->ssr = ssr; x->vs = vs; x->vsnorm = (ssr > 0) ? (vs / ssr) : 0.0;
    x->overdubprev = x->overdubamp = x->speedfloat = 1.0;
    x->snrtype = x->interpflag = 1;
    x->initiallow = x->initialhigh = -1;
    x->ochans = (ochans <= 1) ? 1 : ((ochans == 2) ? 2 : 4);
    x->initskip = 1;
}

void karma_core_set_dims(t_karma *x)
{
    if (!x->bufio.ctx) return;
    x->directionorig = 0;
    x->maxhead = x->playhead = 0.0;
    x->recordhead = -1;
    x->bchans  = x->bufio.chans;
    x->bframes = x->bufio.frames;
    x->bmsr    = x->bufio.sr * 0.001;
    x->bsr     = x->bufio.sr;
    x->nchans  = (x->bchans < x->ochans) ? x->bchans : x->ochans;
    x->srscale = x->bsr / x->ssr;
    x->bvsnorm = x->vsnorm * (x->bsr / (double)x->bframes);
    x->minloop = x->startloop = 0.0;
    x->maxloop = x->endloop   = (x->bframes - 1);
    x->selstart  = 0.0;
    x->selection = 1.0;
}

// Set loop start/end (the pure part of the reference karma_buf_values_internal:
// no buffer~ query, no UI warnings). points_flag: 0 = phase 0..1, 1 = samples,
// 2 = milliseconds. low/high < 0 mean "unset" -> defaults (0 / full). The host
// does any buffer re-query (karma_core_set_dims) before calling this.
void karma_core_set_loop(t_karma *x, double low, double high, long points_flag)
{
    double bframesm1 = (double)(x->bframes - 1);
    double bframesms = bframesm1 / x->bmsr;
    double bvsnorm   = x->vsnorm * (x->bsr / (double)x->bframes);
    double bvsnorm05 = bvsnorm * 0.5;
    x->bvsnorm = bvsnorm;

    if (low < 0.) low = 0.;

    if (points_flag == 0) {                 // phase 0..1 (already normalised)
        if (high < 0.) high = 1.;
    } else if (points_flag == 1) {          // samples
        if (high < 0.) high = 1.; else high = high / bframesm1;
        if (low > 0.) low = low / bframesm1;
    } else {                                // milliseconds
        if (high < 0.) high = 1.; else high = high / bframesms;
        if (low > 0.) low = low / bframesms;
    }

    double lowtemp = low, hightemp = high;  // sort
    low  = (lowtemp < hightemp) ? lowtemp : hightemp;
    high = (lowtemp > hightemp) ? lowtemp : hightemp;

    if (low > 1.)  low = 1. - bvsnorm;
    if (high > 1.) high = 1.;

    if ((high - low) < bvsnorm) {           // enforce minimum loop size (vectorsize)
        if ((high - low) == 0.) return;     // zero-size loop: ignore
        if ((low - bvsnorm05) < 0.)       { low = 0.;  high = bvsnorm; }
        else if ((high + bvsnorm05) > 1.) { high = 1.; low = 1. - bvsnorm; }
        else                              { low -= bvsnorm05; high += bvsnorm05; }
    }

    low  = CLAMP(low, 0., 1.);
    high = CLAMP(high, 0., 1.);

    x->minloop = x->startloop = (int64_t)(low * bframesm1);
    x->maxloop = x->endloop   = (int64_t)(high * bframesm1);

    karma_select_size(x, x->selection);
    karma_select_start(x, x->selstart);
}
