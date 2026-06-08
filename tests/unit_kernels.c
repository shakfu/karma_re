// Unit tests for the pure DSP kernels in karma_core: interpolation macros,
// the ease (fade) functions, the ipoke buffer-fade helpers, and the
// interpolation-index wrap math. These are deterministic, host-free functions,
// so they can be checked directly against hand-computed values.
//
// We #include the core source so the static-inline helpers are reachable.

#include <stdio.h>
#include <math.h>
#include "karma_core.c"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_EQ(a, b, tol) do { \
    double _va = (double)(a), _vb = (double)(b), _d = fabs(_va - _vb); \
    if (_d <= (tol)) { g_pass++; } \
    else { g_fail++; printf("  FAIL %s:%d  %s == %s  (%.17g vs %.17g, |d|=%.3g)\n", \
                            __FILE__, __LINE__, #a, #b, _va, _vb, _d); } \
} while (0)

#define EPS 1e-12

// ---------------------------------------------------------------------------
static void test_linear_interp(void)
{
    CHECK_EQ(LINEAR_INTERP(0.0, 2.0, 6.0), 2.0, EPS);   // f=0 -> first
    CHECK_EQ(LINEAR_INTERP(1.0, 2.0, 6.0), 6.0, EPS);   // f=1 -> second
    CHECK_EQ(LINEAR_INTERP(0.5, 2.0, 6.0), 4.0, EPS);   // midpoint
    CHECK_EQ(LINEAR_INTERP(0.25, 0.0, 4.0), 1.0, EPS);
}

// 4-point cubic/spline must pass through the inner two points (f=0 -> x, f=1 -> y)
// and reproduce a collinear ramp exactly.
static void test_cubic_spline_interp(void)
{
    double w = -1, x = 3, y = 7, z = 2;
    CHECK_EQ(CUBIC_INTERP(0.0, w, x, y, z), x, EPS);
    CHECK_EQ(CUBIC_INTERP(1.0, w, x, y, z), y, EPS);
    CHECK_EQ(SPLINE_INTERP(0.0, w, x, y, z), x, EPS);
    CHECK_EQ(SPLINE_INTERP(1.0, w, x, y, z), y, EPS);

    // collinear points (slope 1): interpolation of a ramp is the ramp
    for (double f = 0.0; f <= 1.0; f += 0.125) {
        CHECK_EQ(CUBIC_INTERP(f, 0.0, 1.0, 2.0, 3.0), 1.0 + f, 1e-9);
        CHECK_EQ(SPLINE_INTERP(f, 0.0, 1.0, 2.0, 3.0), 1.0 + f, 1e-9);
    }
}

// ease_record: updwn=0 fades 0->y1 over [0, globalramp]; updwn=1 fades y1->0.
static void test_ease_record(void)
{
    double y = 2.0, gr = 256.0;
    CHECK_EQ(ease_record(y, 0, gr, 0),   0.0, EPS);   // fade-in start
    CHECK_EQ(ease_record(y, 0, gr, 256), y,   EPS);   // fade-in end
    CHECK_EQ(ease_record(y, 0, gr, 128), 1.0, EPS);   // half-cosine midpoint
    CHECK_EQ(ease_record(y, 1, gr, 0),   y,   EPS);   // fade-out start
    CHECK_EQ(ease_record(y, 1, gr, 256), 0.0, EPS);   // fade-out end
    CHECK_EQ(ease_record(y, 1, gr, 128), 1.0, EPS);
    // symmetry: in(t) + out(t) == y
    for (long pf = 0; pf <= 256; pf += 32)
        CHECK_EQ(ease_record(y, 0, gr, pf) + ease_record(y, 1, gr, pf), y, 1e-9);
}

static void test_ease_switchramp(void)
{
    double y = 2.0;
    // type 0 = linear: y*(1 - snrfade)
    CHECK_EQ(ease_switchramp(y, 0.0, 0), y,   EPS);
    CHECK_EQ(ease_switchramp(y, 1.0, 0), 0.0, EPS);
    CHECK_EQ(ease_switchramp(y, 0.5, 0), 1.0, EPS);
    // type 2 = cubic ease in: y*(1 - snrfade^3)
    CHECK_EQ(ease_switchramp(y, 0.5, 2), y * (1.0 - 0.125), EPS);
    // type 3 = cubic ease out: y*(1 - ((s-1)^3 + 1))
    CHECK_EQ(ease_switchramp(y, 0.5, 3), y * (1.0 - (pow(0.5 - 1, 3) + 1)), EPS);
    // all curves are anchored: snrfade=0 -> full, and bounded within [0,y]
    for (int t = 0; t <= 6; t++) {
        CHECK_EQ(ease_switchramp(y, 0.0, t), y, EPS);
        for (double s = 0.0; s <= 1.0; s += 0.1) {
            double v = ease_switchramp(y, s, t);
            CHECK(v >= -1e-9 && v <= y + 1e-9);
        }
    }
}

// interp_index: 4 indices around playhead with loop wrapping.
static void test_interp_index(void)
{
    t_ptr_int i0, i1, i2, i3;
    long maxloop = 1000, fm1 = 2000;

    // forward, mid-loop: no wrap
    interp_index(100, &i0, &i1, &i2, &i3, /*dir*/ 1, /*orig*/ 0, maxloop, fm1);
    CHECK(i0 == 99 && i1 == 100 && i2 == 101 && i3 == 102);

    // forward, at loop start: i0 wraps to maxloop
    interp_index(0, &i0, &i1, &i2, &i3, 1, 0, maxloop, fm1);
    CHECK(i0 == maxloop && i1 == 0 && i2 == 1 && i3 == 2);

    // forward, at loop end: i2 wraps to 0, i3 follows
    interp_index(maxloop, &i0, &i1, &i2, &i3, 1, 0, maxloop, fm1);
    CHECK(i1 == maxloop && i2 == 0 && i3 == 1);

    // reverse-recorded loop (directionorig < 0): reverse region wrap.
    // loop occupies [fm1-maxloop, fm1] = [1000, 2000]; stepping below 1000 wraps up.
    interp_index(fm1 - maxloop, &i0, &i1, &i2, &i3, /*dir*/ -1, /*orig*/ -1, maxloop, fm1);
    CHECK(i1 == (fm1 - maxloop));
    CHECK(i2 == fm1 - ((fm1 - maxloop) - ((fm1 - maxloop) - 1)));  // == 1999
}

// ease_bufoff: applies a half-cosine fade to globalramp samples from a mark.
static void test_ease_bufoff(void)
{
    enum { N = 64 };
    float b[N];
    for (int i = 0; i < N; i++) b[i] = 1.0f;

    long mark = 10, ramp = 4;
    ease_bufoff(/*framesm1*/ N - 1, b, /*pchans*/ 1, mark, /*dir*/ 1, (double)ramp);

    CHECK_EQ(b[9],  1.0, 1e-6);   // before mark: untouched
    CHECK_EQ(b[10], 0.5 * (1.0 - cos(0.0 * PI / ramp)), 1e-6);          // i=0 -> 0
    CHECK_EQ(b[11], 0.5 * (1.0 - cos(1.0 * PI / ramp)), 1e-6);          // i=1
    CHECK_EQ(b[12], 0.5 * (1.0 - cos(2.0 * PI / ramp)), 1e-6);          // i=2 -> 0.5
    CHECK_EQ(b[13], 0.5 * (1.0 - cos(3.0 * PI / ramp)), 1e-6);          // i=3
    CHECK_EQ(b[14], 1.0, 1e-6);   // after ramp: untouched

    // out-of-range mark must not crash or write
    for (int i = 0; i < N; i++) b[i] = 1.0f;
    ease_bufoff(N - 1, b, 1, /*mark past end*/ N + 100, 1, (double)ramp);
    int unchanged = 1;
    for (int i = 0; i < N; i++) if (b[i] != 1.0f) unchanged = 0;
    CHECK(unchanged);
}

int main(void)
{
    printf("=== kernel unit tests ===\n");
    test_linear_interp();
    test_cubic_spline_interp();
    test_ease_record();
    test_ease_switchramp();
    test_interp_index();
    test_ease_bufoff();
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
