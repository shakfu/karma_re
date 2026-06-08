// Offline Max/MSP stub layer for driving karma~ DSP outside of Max.
//
// Provides just enough of the Max runtime to construct a karma~ object and run
// its perform routines against a malloc'd "buffer~". Types come from the real
// c74 headers; only the *functions* the code calls are stubbed here.
//
// The whole point: a known float buffer + scripted control = deterministic,
// inspectable output we can diff between the reference karma~.c and any refactor.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "max_stub.h"

// ---------------------------------------------------------------------------
// Mock buffer~ : a single global buffer is enough for karma~.
// ---------------------------------------------------------------------------

static mock_buffer g_buf = {0};

void mock_buffer_install(float *data, long frames, long chans, double sr)
{
    g_buf.data   = data;
    g_buf.frames = frames;
    g_buf.chans  = chans;
    g_buf.sr     = sr;
    g_buf.valid  = (data != NULL);
}

mock_buffer *mock_buffer_get(void) { return &g_buf; }

// ---------------------------------------------------------------------------
// buffer~ API
// ---------------------------------------------------------------------------

t_buffer_ref *buffer_ref_new(t_object *self, t_symbol *name) { (void)self; (void)name; return (t_buffer_ref *)&g_buf; }
void          buffer_ref_set(t_buffer_ref *x, t_symbol *name) { (void)x; (void)name; }
t_atom_long   buffer_ref_exists(t_buffer_ref *x) { (void)x; return g_buf.valid ? 1 : 0; }
t_buffer_obj *buffer_ref_getobject(t_buffer_ref *x) { (void)x; return g_buf.valid ? (t_buffer_obj *)&g_buf : NULL; }
t_max_err     buffer_ref_notify(t_buffer_ref *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{ (void)x; (void)s; (void)msg; (void)sender; (void)data; return 0; }

float       *buffer_locksamples(t_buffer_obj *b) { (void)b; return g_buf.data; }
void         buffer_unlocksamples(t_buffer_obj *b) { (void)b; }
t_atom_long  buffer_getframecount(t_buffer_obj *b) { (void)b; return g_buf.frames; }
t_atom_long  buffer_getchannelcount(t_buffer_obj *b) { (void)b; return g_buf.chans; }
double       buffer_getsamplerate(t_buffer_obj *b) { (void)b; return g_buf.sr; }
double       buffer_getmillisamplerate(t_buffer_obj *b) { (void)b; return g_buf.sr * 0.001; }
t_max_err    buffer_setdirty(t_buffer_obj *b) { (void)b; return 0; }
t_symbol    *buffer_name(t_buffer_obj *b) { (void)b; return gensym("mockbuf"); }
void         buffer_view(t_buffer_obj *b) { (void)b; }

// ---------------------------------------------------------------------------
// atoms
// ---------------------------------------------------------------------------

t_atom_long atom_getlong(const t_atom *a) { return a ? a->a_w.w_long : 0; }
double      atom_getfloat(const t_atom *a) { return a ? (a->a_type == A_FLOAT ? a->a_w.w_float : (double)a->a_w.w_long) : 0.0; }
t_symbol   *atom_getsym(const t_atom *a) { return (a && a->a_type == A_SYM) ? a->a_w.w_sym : gensym(""); }
long        atom_gettype(const t_atom *a) { return a ? a->a_type : 0; }
t_max_err   atom_setlong(t_atom *a, t_atom_long v) { if (a) { a->a_type = A_LONG; a->a_w.w_long = v; } return 0; }
t_max_err   atom_setfloat(t_atom *a, double v) { if (a) { a->a_type = A_FLOAT; a->a_w.w_float = v; } return 0; }
t_max_err   atom_setsym(t_atom *a, t_symbol *v) { if (a) { a->a_type = A_SYM; a->a_w.w_sym = v; } return 0; }

// ---------------------------------------------------------------------------
// symbols : tiny intern table
// ---------------------------------------------------------------------------

#define MAX_SYMS 1024
static t_symbol  g_syms[MAX_SYMS];
static int       g_nsyms = 0;

t_symbol *gensym(const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_nsyms; i++)
        if (strcmp(g_syms[i].s_name, s) == 0)
            return &g_syms[i];
    if (g_nsyms >= MAX_SYMS) return &g_syms[0];
    g_syms[g_nsyms].s_name = strdup(s);
    g_syms[g_nsyms].s_thing = NULL;
    return &g_syms[g_nsyms++];
}

// ---------------------------------------------------------------------------
// class / object lifecycle
// ---------------------------------------------------------------------------

static long g_obj_size = 0;

t_class *class_new(const char *name, const method mnew, const method mfree, long size, const method mmenu, short type, ...)
{ (void)name; (void)mnew; (void)mfree; (void)mmenu; (void)type; g_obj_size = size; return (t_class *)calloc(1, 256); }

t_max_err class_addmethod(t_class *c, const method m, const char *name, ...) { (void)c; (void)m; (void)name; return 0; }
t_max_err class_addattr(t_class *c, t_object *attr) { (void)c; (void)attr; return 0; }
t_max_err class_register(t_symbol *name_space, t_class *c) { (void)name_space; (void)c; return 0; }
void      class_dspinit(t_class *c) { (void)c; }

void *object_alloc(t_class *c) { (void)c; return calloc(1, g_obj_size ? (size_t)g_obj_size : (1 << 16)); }
t_max_err object_free(void *x) { free(x); return 0; }
// object_method is a variadic macro -> stub its underlying impl symbol
void *object_method_imp(void *x, void *sym, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6, void *p7, void *p8)
{ (void)x; (void)sym; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5; (void)p6; (void)p7; (void)p8; return NULL; }

void object_error(t_object *x, const char *s, ...) { (void)x; va_list ap; va_start(ap, s); fprintf(stderr, "[karma error] "); vfprintf(stderr, s, ap); fprintf(stderr, "\n"); va_end(ap); }
void object_warn(t_object *x, const char *s, ...)  { (void)x; va_list ap; va_start(ap, s); fprintf(stderr, "[karma warn] ");  vfprintf(stderr, s, ap); fprintf(stderr, "\n"); va_end(ap); }

// ---------------------------------------------------------------------------
// outlets / inlets / clock
// ---------------------------------------------------------------------------

static int g_dummy_outlet, g_dummy_clock;

void *outlet_new(void *x, const char *s) { (void)x; (void)s; return &g_dummy_outlet; }
void *listout(void *x) { (void)x; return &g_dummy_outlet; }

// Capture the most recent list emitted (so tests can inspect the data outlet).
#define MOCK_OUTLET_MAX 32
static t_atom g_outlet_atoms[MOCK_OUTLET_MAX];
static long   g_outlet_count = -1;   // -1 = nothing emitted since last reset

void *outlet_list(void *o, t_symbol *s, short ac, t_atom *av)
{
    (void)o; (void)s;
    g_outlet_count = (ac < MOCK_OUTLET_MAX) ? ac : MOCK_OUTLET_MAX;
    for (long i = 0; i < g_outlet_count; i++) g_outlet_atoms[i] = av[i];
    return NULL;
}
void    mock_outlet_reset(void) { g_outlet_count = -1; }
long    mock_outlet_count(void) { return g_outlet_count; }
double  mock_outlet_value(long i)
{
    if (i < 0 || i >= g_outlet_count) return 0.0;
    t_atom *a = &g_outlet_atoms[i];
    return (a->a_type == A_FLOAT) ? a->a_w.w_float : (double)a->a_w.w_long;
}

void *clock_new(void *obj, method fn) { (void)obj; (void)fn; return &g_dummy_clock; }
void  clock_delay(void *x, long n) { (void)x; (void)n; }
void  clock_unset(void *x) { (void)x; }
void  clock_fdelay(void *x, double n) { (void)x; (void)n; }

// ---------------------------------------------------------------------------
// dsp / attributes / system
// ---------------------------------------------------------------------------

void dsp_setup(t_pxobject *x, long nsignals) { (void)x; (void)nsignals; }
void dsp_free(t_pxobject *x) { (void)x; }

// Index of the first attribute atom (a symbol beginning with '@'); with no
// attributes in the arg list this is simply argc.
long attr_args_offset(short ac, t_atom *av)
{
    for (short i = 0; i < ac; i++)
        if (av[i].a_type == A_SYM && av[i].a_w.w_sym && av[i].a_w.w_sym->s_name[0] == '@')
            return i;
    return ac;
}
void attr_args_process(void *x, short ac, t_atom *av) { (void)x; (void)ac; (void)av; }

// ---------------------------------------------------------------------------
// attribute registration / misc (only exercised by ext_main + message handlers)
// ---------------------------------------------------------------------------

static int g_dummy_attr;

t_object *attr_offset_new(const char *name, const t_symbol *type, long flags, const method mget, const method mset, long offset)
{ (void)name; (void)type; (void)flags; (void)mget; (void)mset; (void)offset; return (t_object *)&g_dummy_attr; }

t_max_err attr_addfilter_clip(void *x, double min, double max, long usemin, long usemax)
{ (void)x; (void)min; (void)max; (void)usemin; (void)usemax; return 0; }

void *class_attr_get(t_class *c, t_symbol *attrname) { (void)c; (void)attrname; return &g_dummy_attr; }
t_max_err class_attr_addattr_format(t_class *c, const char *attrname, const char *attrname2, const t_symbol *type, long flags, const char *fmt, ...) { (void)c; (void)attrname; (void)attrname2; (void)type; (void)flags; (void)fmt; return 0; }
t_max_err class_attr_addattr_parse(t_class *c, const char *attrname, const char *attrname2, t_symbol *type, long flags, const char *parsestr) { (void)c; (void)attrname; (void)attrname2; (void)type; (void)flags; (void)parsestr; return 0; }

void *defer(void *ob, method fn, t_symbol *s, short argc, t_atom *argv) { (void)ob; (void)fn; (void)s; (void)argc; (void)argv; return NULL; }
t_symbol *gensym_tr(const char *s) { return gensym(s); }
long proxy_getinlet(t_object *master) { (void)master; return 0; }

char *strncpy_zero(char *dst, const char *src, long size) { if (size > 0) { strncpy(dst, src, (size_t)size - 1); dst[size - 1] = 0; } return dst; }
int snprintf_zero(char *dst, size_t count, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vsnprintf(dst, count, fmt, ap); va_end(ap); return n; }

t_ptr sysmem_newptrclear(t_ptr_size size) { return (t_ptr)calloc(1, (size_t)(size > 0 ? size : 1)); }
t_ptr sysmem_newptr(t_ptr_size size) { return (t_ptr)malloc((size_t)(size > 0 ? size : 1)); }
void  sysmem_freeptr(void *ptr) { free(ptr); }
void  error(const char *s, ...) { va_list ap; va_start(ap, s); fprintf(stderr, "[karma error] "); vfprintf(stderr, s, ap); fprintf(stderr, "\n"); va_end(ap); }

float sys_getsr(void) { return (float)(g_buf.sr > 0 ? g_buf.sr : 48000.0); }
int   sys_getblksize(void) { return 64; }
int   sys_getdspstate(void) { return 1; }
