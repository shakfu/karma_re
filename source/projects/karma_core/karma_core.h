// karma_core.h -- the standalone (Max-free) build configuration of the core.
//
// This is what karma_core.c includes. It provides Max-free equivalents of the
// few scalar types the copied DSP code expects, no-op shims for the UI/clock
// plumbing the core doesn't own, the interpolation macros, and then the actual
// public API (karma_core_api.h).
//
// A Max host should NOT include this file -- it should include the real c74
// headers (for the scalar types + real clock/outlet/error functions) and then
// karma_core_api.h directly.

#ifndef KARMA_CORE_H
#define KARMA_CORE_H

#include <math.h>
#include <string.h>

// --- scalar types the copied code expects (Max-free equivalents) -----------
// (Integer DSP state uses the standard int64_t directly; karma_core_api.h pulls
// in <stdint.h>. Only the remaining Max-isms below need Max-free stand-ins.)
typedef char  t_bool;
typedef void  t_object;      // perform sig has an (unused) t_object* dsp64
typedef void  t_buffer_obj;  // opaque buffer handle (the host's, via the iface)
#ifndef true
#define true 1
#define false 0
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef CLAMP
#define CLAMP(a, lo, hi) ( (a)>(lo)?( (a)<(hi)?(a):(hi) ):(lo) )
#endif

// The interpolation kernels (LINEAR/CUBIC/SPLINE + interp_index) live in
// karma_interp.h, and the ease/ipoke write kernels in karma_ipoke.h; karma_core.c
// includes both after this header (so the scalar types above are in scope).

// UI/clock plumbing is a no-op in the core (the host owns reporting/inlets).
#define clock_delay(x, n) ((void)0)
#define clock_unset(x)    ((void)0)
#define object_error(...) ((void)0)
#define proxy_getinlet(o) (0L)

#include "karma_core_api.h"

#endif // KARMA_CORE_H
