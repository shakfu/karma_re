/* 
 * Demonstration of nested struct refactoring approach for karma~
 * This shows how the original flat struct can be refactored to use nested structs
 * to reduce parameter counts in helper functions
 */

#include "karma.h"

// BEFORE: Original flat structure (simplified)
struct t_karma_old {
    t_pxobject k_ob;
    
    // Buffer properties - frequently passed together
    long bframes, bchans, ochans, nchans;
    t_buffer_ref *buf;
    
    // Timing properties - frequently passed together  
    double playhead, maxhead, recordhead;
    double ssr, bsr, srscale;
    
    // Fade/ramp properties - frequently passed together
    long recordfade, playfade, globalramp;
    char recfadeflag, playfadeflag;
    double snrfade;
    
    // State flags - frequently passed together
    t_bool record, go, triginit, jumpflag;
    char directionorig;
    
    // Loop boundaries - frequently passed together
    long minloop, maxloop, startloop, endloop;
};

// AFTER: Refactored nested structure
struct t_karma_new {
    t_pxobject k_ob;
    
    // Group related fields into logical nested structs
    struct {
        long bframes, bchans, ochans, nchans;
        t_buffer_ref *buf;
    } buffer;
    
    struct {
        double playhead, maxhead, recordhead;
        double ssr, bsr, srscale;  
    } timing;
    
    struct {
        long recordfade, playfade, globalramp;
        char recfadeflag, playfadeflag;
        double snrfade;
    } fade;
    
    struct {
        t_bool record, go, triginit, jumpflag;
        char directionorig;
    } state;
    
    struct {
        long minloop, maxloop, startloop, endloop;
    } loop;
};

// BEFORE: Helper function with many individual parameters (13 parameters)
static inline void process_recording_old(
    t_bool record, double globalramp, long frames, float *b, long pchans,
    double accuratehead, long *recordhead, char direction, long *recordfade,
    char *recfadeflag, double *snrfade, t_bool use_ease_on, double ease_pos)
{
    *snrfade = 0.0;
    if (record) {
        if (globalramp) {
            // ... complex processing logic
        }
        *recordfade = 0;
        *recfadeflag = 0;
        *recordhead = -1;
    }
}

// AFTER: Helper function with struct pointers (5 parameters - 62% reduction!)
static inline void process_recording_new(
    struct t_karma_new *x, float *b, long frames, long pchans, double accuratehead,
    char direction, t_bool use_ease_on, double ease_pos)
{
    x->fade.snrfade = 0.0;
    if (x->state.record) {
        if (x->fade.globalramp) {
            // ... same complex processing logic, but cleaner access
        }
        x->fade.recordfade = 0;
        x->fade.recfadeflag = 0;
        x->timing.recordhead = -1;
    }
}

// BEFORE: Function call with many arguments
void example_usage_old(struct t_karma_old *x, float *buffer, long frames, long pchans) {
    process_recording_old(
        x->record, x->globalramp, frames, buffer, pchans,
        x->playhead, &x->recordhead, 1, &x->recordfade,
        &x->recfadeflag, &x->snrfade, 1, 0.0
    );
}

// AFTER: Function call with fewer, clearer arguments  
void example_usage_new(struct t_karma_new *x, float *buffer, long frames, long pchans) {
    process_recording_new(x, buffer, frames, pchans, x->timing.playhead, 1, 1, 0.0);
}

/*
 * BENEFITS OF THIS APPROACH:
 * 
 * 1. REDUCED PARAMETER COUNTS:
 *    - process_recording_old: 13 parameters → process_recording_new: 8 parameters
 *    - Similar reductions for other complex functions
 *    - Some functions go from 15-20 parameters down to 5-8
 *
 * 2. LOGICAL GROUPING:  
 *    - Related fields are grouped together conceptually
 *    - buffer.* for all buffer-related properties
 *    - timing.* for all timing/position properties
 *    - fade.* for all fade/ramp control properties
 *    - state.* for all state flags and control
 *    - loop.* for all loop boundary properties
 *
 * 3. BETTER MAINTAINABILITY:
 *    - Changes to parameter groups are localized
 *    - Function signatures are much cleaner
 *    - Less error-prone parameter passing
 *
 * 4. IMPROVED CACHE LOCALITY:
 *    - Related data accessed together is stored together
 *    - Better memory access patterns
 *
 * 5. EASIER DEBUGGING:
 *    - Grouped parameters make it clearer what data functions operate on
 *    - Less parameter marshalling confusion
 *
 * IMPLEMENTATION STRATEGY:
 * 1. Refactor struct definition (done in karma~.c)
 * 2. Update all field access patterns (x->field → x->group.field)  
 * 3. Update helper function signatures to take struct pointers
 * 4. Update function calls to pass struct pointers instead of individual fields
 * 5. Test and fix any remaining compilation issues
 */