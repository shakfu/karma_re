// karma_ipoke.h -- the record/ipoke write engine and its ease/fade kernels.
//
// These are the buffer-write side of the DSP: the per-sample record easing
// (ease_record), the switch-and-ramp declick (ease_switchramp), and the two
// buffer-fade helpers (ease_bufoff / ease_bufon) that declick the buffer at
// record on/off and loop boundaries. They operate only on primitive arguments
// (the host buffer pointer, channel count, frame indices), never on t_karma, so
// they are pure and unit-testable in isolation.
//
// Include AFTER the scalar types + PI are in scope (karma_core.h in the
// standalone build). Kernels are static inline so a single-TU build (and the
// unit tests, which #include karma_core.c) reach them with no link changes.

#ifndef KARMA_IPOKE_H
#define KARMA_IPOKE_H

// easing function for recording (with ipoke)
static inline double ease_record(double y1, char updwn, double globalramp, int64_t playfade)  // !! rewrite !!
{
    double ifup    = (1.0 - (((double)playfade) / globalramp)) * PI;
    double ifdown  = (((double)playfade) / globalramp) * PI;
    return updwn ? y1 * (0.5 * (1.0 - cos(ifup))) : y1 * (0.5 * (1.0 - cos(ifdown)));
}

// easing function for switch & ramp
static inline double ease_switchramp(double y1, double snrfade, int64_t snrtype)
{
    switch (snrtype)
    {
        case 0: y1  = y1 * (1.0 - snrfade);                                             // case 0 = linear
            break;
        case 1: y1  = y1 * (1.0 - (sin((snrfade - 1) * PI/2) + 1));                     // case 1 = sine ease in
            break;
        case 2: y1  = y1 * (1.0 - (snrfade * snrfade * snrfade));                       // case 2 = cubic ease in
            break;
        case 3: snrfade = snrfade - 1;
                y1  = y1 * (1.0 - (snrfade * snrfade * snrfade + 1));                   // case 3 = cubic ease out
            break;
        case 4: snrfade = (snrfade == 0.0) ? snrfade : pow(2, (10 * (snrfade - 1)));
                y1  = y1 * (1.0 - snrfade);                                             // case 4 = exponential ease in
            break;
        case 5: snrfade = (snrfade == 1.0) ? snrfade : (1 - pow(2, (-10 * snrfade)));
                y1  = y1 * (1.0 - snrfade);                                             // case 5 = exponential ease out
            break;
        case 6: if ((snrfade > 0) && (snrfade < 0.5))
                    y1 = y1 * (1.0 - (0.5 * pow(2, ((20 * snrfade) - 10))));
                else if ((snrfade < 1) && (snrfade > 0.5))
                    y1 = y1 * (1.0 - (-0.5 * pow(2, ((-20 * snrfade) + 10)) + 1));      // case 6 = exponential ease in/out
            break;
    }
    return  y1;
}

// easing function for buffer read
static inline void ease_bufoff(int64_t framesm1, float *b, int64_t pchans, int64_t markposition, char direction, double globalramp)
{
    long i, fadpos;

    for (i = 0; i < globalramp; i++)
    {
        fadpos = markposition + (direction * i);

        if ( !((fadpos < 0) || (fadpos > framesm1)) )
        {
            b[fadpos * pchans] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

            if (pchans > 1)
            {
                b[(fadpos * pchans) + 1] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                if (pchans > 2)
                {
                    b[(fadpos * pchans) + 2] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                    if (pchans > 3)
                    {
                        b[(fadpos * pchans) + 3] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));
                    }
                }
            }
        }
    }

    return;
}

// easing function for buffer write
static inline void ease_bufon(int64_t framesm1, float *b, int64_t pchans, int64_t markposition1, int64_t markposition2, char direction, double globalramp)
{
    long i, fadpos1, fadpos2, fadpos3;

    for (i = 0; i < globalramp; i++)
    {
        fadpos1 = (markposition1 + (-direction)) + (-direction * i);
        fadpos2 = (markposition2 + (-direction)) + (-direction * i);
        fadpos3 =  markposition2 + (direction * i);

        if ( !((fadpos1 < 0) || (fadpos1 > framesm1)) )
        {
            b[fadpos1 * pchans] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

            if (pchans > 1)
            {
                b[(fadpos1 * pchans) + 1] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                if (pchans > 2)
                {
                    b[(fadpos1 * pchans) + 2] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                    if (pchans > 3)
                    {
                        b[(fadpos1 * pchans) + 3] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));
                    }
                }
            }
        }

        if ( !((fadpos2 < 0) || (fadpos2 > framesm1)) )
        {
            b[fadpos2 * pchans] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

            if (pchans > 1)
            {
                b[(fadpos2 * pchans) + 1] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                if (pchans > 2)
                {
                    b[(fadpos2 * pchans) + 2] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                    if (pchans > 3)
                    {
                        b[(fadpos2 * pchans) + 3] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));
                    }
                }
            }
        }

        if ( !((fadpos3 < 0) || (fadpos3 > framesm1)) )
        {
            b[fadpos3 * pchans] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

            if (pchans > 1)
            {
                b[(fadpos3 * pchans) + 1] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                if (pchans > 2)
                {
                    b[(fadpos3 * pchans) + 2] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));

                    if (pchans > 3)
                    {
                        b[(fadpos3 * pchans) + 3] *= 0.5 * ( 1.0 - cos( (((double)i) / globalramp) * PI));
                    }
                }
            }
        }
    }

    return;
}

#endif // KARMA_IPOKE_H
