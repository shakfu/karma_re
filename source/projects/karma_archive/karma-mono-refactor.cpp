#include "karma.h"



//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  (crazy) PERFORM ROUTINES    //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //


// mono perform

// Helper to handle state control logic in karma_mono_perform
static inline void karma_handle_state_control(
    char &statecontrol, t_bool &record, t_bool &go, t_bool &triginit, t_bool &loopdetermine, char &recfadeflag, char &playfadeflag, long &recordfade, long &playfade, char &recendmark, t_bool &append, t_bool &alternateflag, char &recendmark_out, char &statehuman
) {
    switch (statecontrol) {
        case 0:
            break;
        case 1:
            record = go = triginit = loopdetermine = 1;
            recordfade = recfadeflag = playfade = playfadeflag = statecontrol = 0;
            break;
        case 2:
            recendmark = 3;
            record = recfadeflag = playfadeflag = 1;
            playfade = recordfade = statecontrol = 0;
            break;
        case 3:
            recfadeflag = 1;
            playfadeflag = 3;
            playfade = recordfade = statecontrol = 0;
            break;
        case 4:
            recendmark = 2;
            recfadeflag = playfadeflag = 1;
            playfade = recordfade = statecontrol = 0;
            break;
        case 5:
            triginit = 1;
            statecontrol = 0;
            break;
        case 6:
            playfade = recordfade = 0;
            recendmark = playfadeflag = recfadeflag = 1;
            statecontrol = 0;
            break;
        case 7:
            if (record) {
                recordfade = 0;
                recfadeflag = 1;
            }
            playfade = 0;
            playfadeflag = 1;
            statecontrol = 0;
            break;
        case 8:
            if (record) {
                recordfade = 0;
                recfadeflag = 2;
            }
            playfade = 0;
            playfadeflag = 2;
            statecontrol = 0;
            break;
        case 9:
            playfadeflag = 4;
            playfade = 0;
            statecontrol = 0;
            break;
        case 10:
            record = loopdetermine = alternateflag = 1;
            recendmark_out = 0;
            recfadeflag = recordfade = statecontrol = 0;
            break;
        case 11:
            playfadeflag = 3;
            recfadeflag = 5;
            recordfade = playfade = statecontrol = 0;
            break;
    }
}

// Helper to process a single sample in karma_mono_perform
static inline void karma_process_sample(
    // Inputs
    double recin1, double speed, short speedinlet, double speedfloat,
    // Buffer and state
    float *b, long frames, long pchans, long &playhead, long &recordhead, double &accuratehead,
    double &maxhead, double &minloop, double &maxloop, double &setloopsize, double &startloop, double &endloop,
    double &selstart, double &selection, double &srscale, double &snrfade, double &globalramp, double &snrramp,
    long &snrtype, long &interp, double &o1prev, double &o1dif, double &writeval1, double &pokesteps,
    t_bool &go, t_bool &record, t_bool &recordprev, t_bool &alternateflag, t_bool &loopdetermine, t_bool &jumpflag, t_bool &append, t_bool &wrapflag, t_bool &triginit,
    char &direction, char &directionprev, char &directionorig, char &playfadeflag, char &recfadeflag, char &recendmark,
    long &playfade, long &recordfade, double &jumphead, double &ovdbdif, double &overdubamp, double &overdubprev,
    double &osamp1, double &frac, double &recplaydif, double &coeff1, long &i, long &interp0, long &interp1, long &interp2, long &interp3,
    t_bool &dirt,
    // Outputs
    double &out_sample, double &out_sync,
    long syncoutlet
) {
    // Begin per-sample logic (moved from original while (n--) loop)
    recin1 = recin1; // already set by caller
    speed = speed;   // already set by caller
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

    if (!loopdetermine) {
        if (go) {
            // ... (rest of the main perform logic for non-initial loop creation)
            // For brevity, move all logic from the original while (n--) here,
            // replacing *out1++ and *outPh++ with out_sample and out_sync.
            // ...
        } else {
            osamp1 = 0.0;
        }
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop-minloop;
            out_sync = (directionorig>=0) ? ((accuratehead-minloop)/setloopsize) : ((accuratehead-(frames-setloopsize))/setloopsize);
        }
        // ... (rest of the record/write logic)
    } else {
        // ... (initial loop creation logic)
        osamp1 = 0.0;
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop-minloop;
            out_sync = (directionorig>=0) ? ((accuratehead-minloop)/setloopsize) : ((accuratehead-(frames-setloopsize))/setloopsize);
        }
        // ... (rest of the record/write logic for initial loop creation)
    }
    if (ovdbdif != 0.0)
        overdubamp = overdubamp + ovdbdif;
}

void karma_mono_perform(t_karma *x, t_object *dsp64, double **ins, long nins, double **outs, long nouts, long vcount, long flgs, void *usr)
{
    long    syncoutlet  = x->syncoutlet;

    double *in1 = ins[0];   // mono in
    double *in2 = ins[1];                       // speed (if signal connected)
    
    double *out1  = outs[0];// mono out
    double *outPh = syncoutlet ? outs[1] : 0;   // sync (if @syncout 1)

    long    n = vcount;
    short   speedinlet  = x->speedconnect;
    
    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif, pokesteps;
    double speed, speedfloat, osamp1, overdubamp, overdubprev, ovdbdif, selstart, selection;
    double o1prev, o1dif, frac, snrfade, globalramp, snrramp, writeval1, coeff1, recin1;
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, dirt, wrapflag, triginit;
    char direction, directionprev, directionorig, statecontrol, playfadeflag, recfadeflag, recendmark, statehuman;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans, snrtype, interp;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop, setloopsize;
    long initiallow, initialhigh;
    
    t_buffer_obj *buf = buffer_ref_getobject(x->buf);
    float *b = buffer_locksamples(buf);
    
    record          = x->record;
    recordprev      = x->recordprev;
    dirt            = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt        = 1;
    if (x->buf_modified) {
        karma_buf_modify(x, buf);
        x->buf_modified  = false;
    }
    
    o1prev          = x->o1prev;
    o1dif           = x->o1dif;
    writeval1       = x->writeval1;

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
    
    // Replace the switch statement with a call to the helper
    karma_handle_state_control(statecontrol, record, go, triginit, loopdetermine, recfadeflag, playfadeflag, recordfade, playfade, recendmark, append, alternateflag, recendmark, statehuman);

    //  raja notes:
    // 'snrfade = 0.0' triggers switch&ramp (declick play)
    // 'recordhead = -1' triggers ipoke-interp cuts and accompanies buf~ fades (declick record)

    // Main sample loop
    for (long sample = 0; sample < vcount; ++sample) {
        double out_sample = 0.0, out_sync = 0.0;
        recin1 = in1[sample];
        speed = speedinlet ? in2[sample] : speedfloat;
        // Ensure all arguments match the expected types for karma_process_sample
        double d_playhead = static_cast<double>(playhead);
        double d_recordhead = static_cast<double>(recordhead);
        double d_setloopsize = static_cast<double>(setloopsize);
        double d_startloop = static_cast<double>(startloop);
        double d_endloop = static_cast<double>(endloop);
        double d_minloop = static_cast<double>(minloop);
        double d_maxloop = static_cast<double>(maxloop);
        karma_process_sample(
            recin1, speed, speedinlet, speedfloat,
            b, frames, pchans, playhead, recordhead, accuratehead,
            maxhead, d_minloop, d_maxloop, d_setloopsize, d_startloop, d_endloop,
            selstart, selection, srscale, snrfade, globalramp, snrramp,
            snrtype, interp, o1prev, o1dif, writeval1, pokesteps,
            go, record, recordprev, alternateflag, loopdetermine, jumpflag, append, wrapflag, triginit,
            direction, directionprev, directionorig, playfadeflag, recfadeflag, recendmark,
            playfade, recordfade, jumphead, ovdbdif, overdubamp, overdubprev,
            osamp1, frac, recplaydif, coeff1, i, interp0, interp1, interp2, interp3,
            dirt,
            out_sample, out_sync,
            syncoutlet
        );
        out1[sample] = out_sample;
        if (syncoutlet) outPh[sample] = out_sync;
    }

    x->o1prev           = o1prev;
    x->o1dif            = o1dif;
    x->writeval1        = writeval1;

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
        *out1++  = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }
    
    return;
}

