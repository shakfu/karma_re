// karma_interp.h -- the buffer-read interpolation kernels.
//
// The three fractional-interpolation forms (linear / cubic / spline) the perform
// routines pick between, plus interp_index() which computes the four neighbour
// sample indices (indx0..indx3) around a playhead, wrapping them within the loop
// according to playback direction. Pure: operates on indices and a frac, never
// on t_karma.
//
// Include AFTER the scalar types are in scope (karma_core.h in the standalone
// build). interp_index is static inline so a single-TU build (and the unit
// tests, which #include karma_core.c) reach it with no link changes.

#ifndef KARMA_INTERP_H
#define KARMA_INTERP_H

// Fractional interpolation kernels (verbatim from the reference)
#define LINEAR_INTERP(f, x, y) (x + f*(y - x))
#define CUBIC_INTERP(f, w, x, y, z) ((((0.5*(z - w) + 1.5*(x - y))*f + (w - 2.5*x + y + y - 0.5*z))*f + (0.5*(y - w)))*f + x)
#define SPLINE_INTERP(f, w, x, y, z) (((-0.5*w + 1.5*x - 1.5*y + 0.5*z)*f*f*f) + ((w - 2.5*x + y + y - 0.5*z)*f*f) + ((-0.5*w + 0.5*y)*f) + x)

// interpolation points
static inline void interp_index(int64_t playhead, int64_t *indx0, int64_t *indx1, int64_t *indx2, int64_t *indx3, char direction, char directionorig, int64_t maxloop, int64_t framesm1)
{
    *indx0 = playhead - direction;                                   // calc of indecies for interpolations

    if (directionorig >= 0) {
        if (*indx0 < 0) {
            *indx0 = (maxloop + 1) + *indx0;
        } else if (*indx0 > maxloop) {
            *indx0 = *indx0 - (maxloop + 1);
        }
    } else {
        if(*indx0 < (framesm1 - maxloop)) {
            *indx0 = framesm1 - ((framesm1 - maxloop) - *indx0);
        } else if (*indx0 > framesm1) {
            *indx0 = (framesm1 - maxloop) + (*indx0 - framesm1);
        }
    }

    *indx1 = playhead;
    *indx2 = playhead + direction;

    if (directionorig >= 0) {
        if (*indx2 < 0) {
            *indx2 = (maxloop + 1) + *indx2;
        } else if (*indx2 > maxloop) {
            *indx2 = *indx2 - (maxloop + 1);
        }
    } else {
        if (*indx2 < (framesm1 - maxloop)) {
            *indx2 = framesm1 - ((framesm1 - maxloop) - *indx2);
        } else if (*indx2 > framesm1) {
            *indx2 = (framesm1 - maxloop) + (*indx2 - framesm1);
        }
    }

    *indx3 = *indx2 + direction;

    if (directionorig >= 0) {
        if(*indx3 < 0) {
            *indx3 = (maxloop + 1) + *indx3;
        } else if (*indx3 > maxloop) {
            *indx3 = *indx3 - (maxloop + 1);
        }
    } else {
        if (*indx3 < (framesm1 - maxloop)) {
            *indx3 = framesm1 - ((framesm1 - maxloop) - *indx3);
        } else if (*indx3 > framesm1) {
            *indx3 = (framesm1 - maxloop) + (*indx3 - framesm1);
        }
    }

    return;
}

#endif // KARMA_INTERP_H
