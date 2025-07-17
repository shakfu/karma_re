#include "karma.h"

// Helper to handle state control logic in karma_mono_perform
static inline void karma_handle_state_control(
    control_state& statecontrol, t_bool& record, t_bool& go, t_bool& triginit,
    t_bool& loopdetermine, char& recfadeflag, char& playfadeflag,
    long& recordfade, long& playfade, char& recendmark, t_bool& append,
    t_bool& alternateflag, char& recendmark_out, human_state& statehuman)
{
    switch (statecontrol)   // "all-in-one 'switch' statement to catch and handle all(most) messages" - raja
    {
        case CONTROL_STATE_ZERO:
            // case 0: zero
            break;
        case CONTROL_STATE_RECORD_INITIAL_LOOP:
            // case 1: record initial loop
            record = go = triginit = loopdetermine = 1;
            statecontrol = CONTROL_STATE_ZERO;
            recordfade = recfadeflag = playfade = playfadeflag = 0;
            break;
        case CONTROL_STATE_RECORD_ALTERNATE:
            // case 2: record alternateflag (wtf is 'alternateflag' ('rectoo') ?!)
            // in to OVERDUB ??
            recendmark = 3;
            record = recfadeflag = playfadeflag = 1;
            statecontrol = CONTROL_STATE_ZERO;
            playfade = recordfade = 0;
            break;
        case CONTROL_STATE_RECORD_OFF:
            // case 3: record off regular
            recfadeflag = 1;
            playfadeflag = 3;
            statecontrol = CONTROL_STATE_ZERO;
            playfade = recordfade = 0;
            break;
        case CONTROL_STATE_PLAY_ALTERNATE:
            // case 4: play alternateflag (wtf is 'alternateflag' ('rectoo') ?!)
            // out of OVERDUB ??
            recendmark = 2;
            recfadeflag = playfadeflag = 1;
            statecontrol = CONTROL_STATE_ZERO;
            playfade = recordfade = 0;
            break;
        case CONTROL_STATE_PLAY_ON:
            // case 5: play on regular
            triginit = 1;   // ?!?!
            statecontrol = CONTROL_STATE_ZERO;
            break;
        case CONTROL_STATE_STOP_ALTERNATE:
            // case 6: stop alternateflag (wtf is 'alternateflag' ('rectoo') ?!)
            // after OVERDUB ??
            playfade = recordfade = 0;
            recendmark = playfadeflag = recfadeflag = 1;
            statecontrol = CONTROL_STATE_ZERO;
            break;
        case CONTROL_STATE_STOP:
            // case 7: stop regular
            if (record) {
                recordfade = 0;
                recfadeflag = 1;
            }
            playfade = 0;
            playfadeflag = 1;
            statecontrol = CONTROL_STATE_ZERO;
            break;
        case CONTROL_STATE_JUMP:
            // case 8: jump
            if (record) {
                recordfade = 0;
                recfadeflag = 2;
            }
            playfade = 0;
            playfadeflag = 2;
            statecontrol = CONTROL_STATE_ZERO;
            break;
        case CONTROL_STATE_APPEND:
            // case 9: append
            playfadeflag = 4;   // !! modified in perform loop switch case(s) for playing behind append
            playfade = 0;
            statecontrol = CONTROL_STATE_ZERO;
            break;
        case CONTROL_STATE_APPEND_SPECIAL:
            // case 10: special case append (what is special about it ?!)   // in to RECORD / OVERDUB ??
            record = loopdetermine = alternateflag = 1;
            // snrfade = 0.0;
            statecontrol = CONTROL_STATE_ZERO;
            recordfade = recfadeflag = 0;
            break;
        case CONTROL_STATE_RECORD_ON:
            // case 11: record on regular (when ?! not looped ?!)
            playfadeflag = 3;
            recfadeflag = 5;
            statecontrol = CONTROL_STATE_ZERO;
            recordfade = playfade = 0;
            break;          // !!
    }
}

// Helper to process a single sample in karma_mono_perform
static inline void karma_process_sample(
    // Inputs
    double recin1, double speed, short speedinlet, double speedfloat,
    // Buffer and state
    float* b, long frames, long pchans, long& playhead, long& recordhead,
    double& accuratehead, double& maxhead, double& minloop, double& maxloop,
    double& setloopsize, double& startloop, double& endloop, double& selstart,
    double& selection, double& srscale, double& snrfade, double& globalramp,
    double& snrramp, long& snrtype, long& interp, double& o1prev, double& o1dif,
    double& writeval1, double& pokesteps, t_bool& go, t_bool& record,
    t_bool& recordprev, t_bool& alternateflag, t_bool& loopdetermine,
    t_bool& jumpflag, t_bool& append, t_bool& wrapflag, t_bool& triginit,
    char& direction, char& directionprev, char& directionorig,
    char& playfadeflag, char& recfadeflag, char& recendmark, long& playfade,
    long& recordfade, double& jumphead, double& ovdbdif, double& overdubamp,
    double& overdubprev, double& osamp1, double& frac, double& recplaydif,
    double& coeff1, long& i, long& interp0, long& interp1, long& interp2,
    long& interp3, t_bool& dirt,
    // Outputs
    double& out_sample, double& out_sync, long syncoutlet)
{
    double speedsrscaled;
    // Begin per-sample logic (moved from original while (n--) loop)
    recin1 = recin1; // already set by caller
    speed = speed;   // already set by caller
    direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

    // declick for change of 'dir'ection
    if (directionprev != direction) {
        if (record && globalramp) {
            ease_bufoff(
                frames - 1, b, pchans, recordhead, -direction, globalramp);
            recordfade = recfadeflag = 0;
            recordhead = -1;
        }
        snrfade = 0.0;
    }

    if ((record - recordprev) < 0) { // samp @record-off
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, recordhead, direction, globalramp);
        recordhead = -1;
        dirt = 1;
    } else if ((record - recordprev) > 0) { // samp @record-on
        recordfade = recfadeflag = 0;
        if (speed < 1.0)
            snrfade = 0.0;
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, accuratehead, -direction, globalramp);
    }
    recordprev = record;

    if (!loopdetermine) {
        if (go) {
            // ... (rest of the main perform logic for non-initial loop
            // creation) For brevity, move all logic from the original while
            // (n--) here, replacing *out1++ and *outPh++ with out_sample and
            // out_sync.
            // ...
            // 
    
            /*
            calculate_head(directionorig, maxhead, frames, minloop, selstart, selection, direction, globalramp, &b, pchans, record, jumpflag, jumphead, &maxloop, &setloopsize, &accuratehead, &startloop, &endloop, &wrapflag, &recordhead, &snrfade, &append, &alternateflag, &recendmark, &triginit, &speedsrscaled, &recordfade, &recfadeflag);
            */
            
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
                        // NOTUSED: accuratehead = endloop = startloop + (selection * setloopsize);         // startloop + (selection * maxloop);
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

            /* calculate_head() to here */
            
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
            
            if (record) {           // if recording do linear-interp else...
                osamp1 =    linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
            } else {                // ...cubic / spline if interpflag > 0 (default cubic)
                if (interp == 1)
                    osamp1  = cubic_interp(frac, b[interp0 * pchans], b[interp1 * pchans], b[interp2 * pchans], b[interp3 * pchans]);
                else if (interp == 2)
                    osamp1  = spline_interp(frac, b[interp0 * pchans], b[interp1 * pchans], b[interp2 * pchans], b[interp3 * pchans]);
                else
                    osamp1  = linear_interp(frac, b[interp1 * pchans], b[interp2 * pchans]);
            }
            
            if (globalramp)
            {                                           // "Switch and Ramp" - http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
                if (snrfade < 1.0)
                {
                    if (snrfade == 0.0) {
                        o1dif = o1prev - osamp1;
                    }
                    osamp1 += ease_switchramp(o1dif, snrfade, snrtype);// <- easing-curv options implemented by raja
                    snrfade += 1 / snrramp;
                }                                               // "Switch and Ramp" end
                
                if (playfade < globalramp)
                {                                               // realtime ramps for play on/off
                    osamp1 = ease_record(osamp1, (playfadeflag > 0), globalramp, playfade);
                    playfade++;
                    if (playfade >= globalramp)
                    {
                        switch (playfadeflag)
                        {
                            case 0:
                                break;
                            case 1:
                                playfadeflag = go = 0;  // record alternateflag   // play alternateflag  // stop alternateflag / regular
                                break;
                            case 2:
                                if (!record)
                                    triginit = jumpflag = 1;
//                                  break;                  // !! no break - pass 2 -> 3 !!
                            case 3:                     // jump // record off reg
                                playfadeflag = playfade = 0;
                                break;
                            case 4:                     // append
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
                    case 0:
                        break;
                    case 1:
                        playfadeflag = go = 0;
                        break;
                    case 2:
                        if (!record)
                            triginit = jumpflag = 1;
//                          break;                                  // !! no break - pass 2 -> 3 !!
                    case 3:                                     // jump     // record off reg
                        playfadeflag = 0;
                        break;
                    case 4:                                     // append
                        go = triginit = loopdetermine = 1;
                        // !! will disbling this enable play behind append ?? should this be based on passing previous maxloop ??
                        snrfade = 0.0;
                        playfade = 0;   // !!
                        playfadeflag = 0;
                        break;
                }
            }
                


        } else {
            osamp1 = 0.0;
        }
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop - minloop;
            out_sync = (directionorig >= 0)
                ? ((accuratehead - minloop) / setloopsize)
                : ((accuratehead - (frames - setloopsize)) / setloopsize);
        }
        // ... (rest of the record/write logic)
    } else {
        // ... (initial loop creation logic)
        osamp1 = 0.0;
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop - minloop;
            out_sync = (directionorig >= 0)
                ? ((accuratehead - minloop) / setloopsize)
                : ((accuratehead - (frames - setloopsize)) / setloopsize);
        }
        // ... (rest of the record/write logic for initial loop creation)
    }
    if (ovdbdif != 0.0)
        overdubamp = overdubamp + ovdbdif;
}


//////////////////////// (crazy) PERFORM ROUTINES

// mono perform

// Helper to handle state control logic in karma_mono_perform
static inline void karma_handle_state_control(
    char& statecontrol, t_bool& record, t_bool& go, t_bool& triginit,
    t_bool& loopdetermine, char& recfadeflag, char& playfadeflag,
    long& recordfade, long& playfade, char& recendmark, t_bool& append,
    t_bool& alternateflag, char& recendmark_out, char& statehuman)
{
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
    float* b, long frames, long pchans, long& playhead, long& recordhead,
    double& accuratehead, double& maxhead, double& minloop, double& maxloop,
    double& setloopsize, double& startloop, double& endloop, double& selstart,
    double& selection, double& srscale, double& snrfade, double& globalramp,
    double& snrramp, long& snrtype, long& interp, double& o1prev, double& o1dif,
    double& writeval1, double& pokesteps, t_bool& go, t_bool& record,
    t_bool& recordprev, t_bool& alternateflag, t_bool& loopdetermine,
    t_bool& jumpflag, t_bool& append, t_bool& wrapflag, t_bool& triginit,
    char& direction, char& directionprev, char& directionorig,
    char& playfadeflag, char& recfadeflag, char& recendmark, long& playfade,
    long& recordfade, double& jumphead, double& ovdbdif, double& overdubamp,
    double& overdubprev, double& osamp1, double& frac, double& recplaydif,
    double& coeff1, long& i, long& interp0, long& interp1, long& interp2,
    long& interp3, t_bool& dirt,
    // Outputs
    double& out_sample, double& out_sync, long syncoutlet)
{
    // Begin per-sample logic (moved from original while (n--) loop)
    recin1 = recin1; // already set by caller
    speed = speed;   // already set by caller
    direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

    // declick for change of 'dir'ection
    if (directionprev != direction) {
        if (record && globalramp) {
            ease_bufoff(
                frames - 1, b, pchans, recordhead, -direction, globalramp);
            recordfade = recfadeflag = 0;
            recordhead = -1;
        }
        snrfade = 0.0;
    }

    if ((record - recordprev) < 0) { // samp @record-off
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, recordhead, direction, globalramp);
        recordhead = -1;
        dirt = 1;
    } else if ((record - recordprev) > 0) { // samp @record-on
        recordfade = recfadeflag = 0;
        if (speed < 1.0)
            snrfade = 0.0;
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, accuratehead, -direction, globalramp);
    }
    recordprev = record;

    if (!loopdetermine) {
        if (go) {
            // ... (rest of the main perform logic for non-initial loop
            // creation) For brevity, move all logic from the original while
            // (n--) here, replacing *out1++ and *outPh++ with out_sample and
            // out_sync.
            // ...
        } else {
            osamp1 = 0.0;
        }
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop - minloop;
            out_sync = (directionorig >= 0)
                ? ((accuratehead - minloop) / setloopsize)
                : ((accuratehead - (frames - setloopsize)) / setloopsize);
        }
        // ... (rest of the record/write logic)
    } else {
        // ... (initial loop creation logic)
        osamp1 = 0.0;
        o1prev = osamp1;
        out_sample = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop - minloop;
            out_sync = (directionorig >= 0)
                ? ((accuratehead - minloop) / setloopsize)
                : ((accuratehead - (frames - setloopsize)) / setloopsize);
        }
        // ... (rest of the record/write logic for initial loop creation)
    }
    if (ovdbdif != 0.0)
        overdubamp = overdubamp + ovdbdif;
}

void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs,
    long nouts, long vcount, long flgs, void* usr)
{
    long syncoutlet = x->syncoutlet;

    double* in1 = ins[0]; // mono in
    double* in2 = ins[1]; // speed (if signal connected)

    double* out1 = outs[0];                   // mono out
    double* outPh = syncoutlet ? outs[1] : 0; // sync (if @syncout 1)

    long  n = vcount;
    short speedinlet = x->speedconnect;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif,
        pokesteps;
    double speed, speedfloat, osamp1, overdubamp, overdubprev, ovdbdif,
        selstart, selection;
    double o1prev, o1dif, frac, snrfade, globalramp, snrramp, writeval1, coeff1,
        recin1;
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag,
        append, dirt, wrapflag, triginit;
    char direction, directionprev, directionorig, statecontrol, playfadeflag,
        recfadeflag, recendmark, statehuman;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans,
        snrtype, interp;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop,
        setloopsize;
    long initiallow, initialhigh;

    t_buffer_obj* buf = buffer_ref_getobject(x->buf);
    float*        b = buffer_locksamples(buf);

    record = x->record;
    recordprev = x->recordprev;
    dirt = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt = 1;
    if (x->buf_modified) {
        karma_buf_modify(x, buf);
        x->buf_modified = false;
    }

    o1prev = x->o1prev;
    o1dif = x->o1dif;
    writeval1 = x->writeval1;

    go = x->go;
    statecontrol = x->statecontrol;
    playfadeflag = x->playfadeflag;
    recfadeflag = x->recfadeflag;
    recordhead = x->recordhead;
    alternateflag = x->alternateflag;
    pchans = x->bchans;
    srscale = x->srscale;
    frames = x->bframes;
    triginit = x->triginit;
    jumpflag = x->jumpflag;
    append = x->append;
    directionorig = x->directionorig;
    directionprev = x->directionprev;
    minloop = x->minloop;
    maxloop = x->maxloop;
    initiallow = x->initiallow;
    initialhigh = x->initialhigh;
    selection = x->selection;
    loopdetermine = x->loopdetermine;
    startloop = x->startloop;
    selstart = x->selstart;
    endloop = x->endloop;
    recendmark = x->recendmark;
    overdubamp = x->overdubprev;
    overdubprev = x->overdubamp;
    ovdbdif = (overdubamp != overdubprev) ? ((overdubprev - overdubamp) / n)
                                          : 0.0;
    recordfade = x->recordfade;
    playfade = x->playfade;
    accuratehead = x->playhead;
    playhead = trunc(accuratehead);
    maxhead = x->maxhead;
    wrapflag = x->wrapflag;
    jumphead = x->jumphead;
    pokesteps = x->pokesteps;
    snrfade = x->snrfade;
    globalramp = (double)x->globalramp;
    snrramp = (double)x->snrramp;
    snrtype = x->snrtype;
    interp = x->interpflag;
    speedfloat = x->speedfloat;

    // Replace the switch statement with a call to the helper
    karma_handle_state_control(
        statecontrol, record, go, triginit, loopdetermine, recfadeflag,
        playfadeflag, recordfade, playfade, recendmark, append, alternateflag,
        recendmark, statehuman);

    //  raja notes:
    // 'snrfade = 0.0' triggers switch&ramp (declick play)
    // 'recordhead = -1' triggers ipoke-interp cuts and accompanies buf~ fades
    // (declick record)

    // Main sample loop
    for (long sample = 0; sample < vcount; ++sample) {
        double out_sample = 0.0, out_sync = 0.0;
        recin1 = in1[sample];
        speed = speedinlet ? in2[sample] : speedfloat;
        // Ensure all arguments match the expected types for
        // karma_process_sample
        double d_playhead = static_cast<double>(playhead);
        double d_recordhead = static_cast<double>(recordhead);
        double d_setloopsize = static_cast<double>(setloopsize);
        double d_startloop = static_cast<double>(startloop);
        double d_endloop = static_cast<double>(endloop);
        double d_minloop = static_cast<double>(minloop);
        double d_maxloop = static_cast<double>(maxloop);
        karma_process_sample(
            recin1, speed, speedinlet, speedfloat, b, frames, pchans, playhead,
            recordhead, accuratehead, maxhead, d_minloop, d_maxloop,
            d_setloopsize, d_startloop, d_endloop, selstart, selection, srscale,
            snrfade, globalramp, snrramp, snrtype, interp, o1prev, o1dif,
            writeval1, pokesteps, go, record, recordprev, alternateflag,
            loopdetermine, jumpflag, append, wrapflag, triginit, direction,
            directionprev, directionorig, playfadeflag, recfadeflag, recendmark,
            playfade, recordfade, jumphead, ovdbdif, overdubamp, overdubprev,
            osamp1, frac, recplaydif, coeff1, i, interp0, interp1, interp2,
            interp3, dirt, out_sample, out_sync, syncoutlet);
        out1[sample] = out_sample;
        if (syncoutlet)
            outPh[sample] = out_sync;
    }

    x->o1prev = o1prev;
    x->o1dif = o1dif;
    x->writeval1 = writeval1;

    x->maxhead = maxhead;
    x->pokesteps = pokesteps;
    x->wrapflag = wrapflag;
    x->snrfade = snrfade;
    x->playhead = accuratehead;
    x->directionorig = directionorig;
    x->directionprev = directionprev;
    x->recordhead = recordhead;
    x->alternateflag = alternateflag;
    x->recordfade = recordfade;
    x->triginit = triginit;
    x->jumpflag = jumpflag;
    x->go = go;
    x->record = record;
    x->recordprev = recordprev;
    x->statecontrol = statecontrol;
    x->playfadeflag = playfadeflag;
    x->recfadeflag = recfadeflag;
    x->playfade = playfade;
    x->minloop = minloop;
    x->maxloop = maxloop;
    x->initiallow = initiallow;
    x->initialhigh = initialhigh;
    x->loopdetermine = loopdetermine;
    x->startloop = startloop;
    x->endloop = endloop;
    x->overdubprev = overdubamp;
    x->recendmark = recendmark;
    x->append = append;

    return;

zero:
    while (n--) {
        *out1++ = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }

    return;
}
