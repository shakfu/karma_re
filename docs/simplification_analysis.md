# K4~ Simplification Analysis

## Current State Assessment

**File Structure:**
- Main file: k4~.cpp (2,503 lines)
- Header files: 22 headers (~2,000+ lines)
- Total: ~4,500 lines (no reduction from original)

**Problem:** We've done mechanical extraction (moving code) rather than true refactoring (simplifying code).

## Major Opportunities for TRUE Simplification

### 1. Unify the Three Perform Functions (HIGH IMPACT)

**Current duplication:**
- `karma_mono_perform()` - 421 lines
- `karma_stereo_perform()` - 443 lines
- `karma_poly_perform()` - 442 lines
- **Total: 1,306 lines of ~90% duplicated logic**

**Analysis of differences:**

```cpp
// MONO
double* in1 = ins[0];           // 1 input
double* out1 = outs[0];         // 1 output
double osamp1;                  // 1 output sample
recin1 = *in1++;                // read 1 channel

// STEREO
double* in1 = ins[0];           // 2 inputs
double* in2 = ins[1];
double* out1 = outs[0];         // 2 outputs
double* out2 = outs[1];
double osamp1, osamp2;          // 2 output samples
recin1 = *in1++;                // read 2 channels
recin2 = *in2++;

// POLY
t_double* in = (t_double*)(ins[0]);    // multichannel input
t_double* out = (t_double*)(outs[0]);  // multichannel output
double* poly_osamp = x->poly_arrays->osamp;   // N output samples
for (c = 0; c < ochans; c++)           // read N channels
    poly_recin[c] = in[(n * ochans) + c];
```

**The ONLY differences:**
1. Number of input/output pointers (1 vs 2 vs N)
2. Number of sample variables (osamp1 vs osamp1/osamp2 vs poly_osamp[])
3. Recording logic calls different iPoke variants
4. Interpolation logic duplicated

**Everything else is IDENTICAL:**
- State machine processing (100+ lines)
- Loop boundary handling (80+ lines)
- Direction changes (40+ lines)
- Jump logic (30+ lines)
- Fade processing (50+ lines)
- Buffer validation (20+ lines)

### Strategy 1A: Template-Based Unification

```cpp
template<int CHANNELS>
void karma_perform_template(
    t_karma* x, t_object* dsp64, double** ins, long nins,
    double** outs, long nouts, long vcount, long flgs, void* usr)
{
    // All the shared logic (90% of code)
    // ...

    // Channel-specific operations using if constexpr (C++17)
    if constexpr (CHANNELS == 1) {
        // Mono: osamp1 = interpolate(...)
        // Mono: iPoke single channel
    } else if constexpr (CHANNELS == 2) {
        // Stereo: osamp1/osamp2 = interpolate(...)
        // Stereo: iPoke two channels
    } else {
        // Poly: poly_osamp[c] = interpolate(...)
        // Poly: iPoke N channels
    }
}

// Thin wrappers
void karma_mono_perform(...) {
    karma_perform_template<1>(x, dsp64, ins, nins, outs, nouts, vcount, flgs, usr);
}
void karma_stereo_perform(...) {
    karma_perform_template<2>(x, dsp64, ins, nins, outs, nouts, vcount, flgs, usr);
}
void karma_poly_perform(...) {
    karma_perform_template<-1>(x, dsp64, ins, nins, outs, nouts, vcount, flgs, usr);
}
```

**Benefits:**
- **Reduce 1,306 lines to ~600 lines** (50% reduction)
- Changes only need to be made once
- Compiler optimizes away runtime branches
- Zero performance penalty

### Strategy 1B: Channel Abstraction Class

```cpp
// Abstract away channel differences
class ChannelIO {
public:
    virtual double read_input(int channel) = 0;
    virtual void write_output(int channel, double sample) = 0;
    virtual int channel_count() = 0;
};

class MonoIO : public ChannelIO { ... };
class StereoIO : public ChannelIO { ... };
class PolyIO : public ChannelIO { ... };

void karma_perform_unified(t_karma* x, ChannelIO& io, ...) {
    // Single implementation, 100% shared
    for (int c = 0; c < io.channel_count(); c++) {
        recin[c] = io.read_input(c);
        // ... processing ...
        io.write_output(c, osamp[c]);
    }
}
```

**Trade-offs:**
- More elegant abstraction
- Slight virtual function overhead (probably negligible)
- Easier to test in isolation

---

### 2. Unify iPoke Recording Variants (MEDIUM IMPACT)

**Current duplication:**
- `process_ipoke_recording()` - mono regular (recording_dsp.hpp)
- `process_initial_loop_ipoke_recording()` - mono initial (initial_loop.hpp)
- `process_ipoke_recording_stereo()` - stereo regular (stereo_recording.hpp)
- `process_initial_loop_ipoke_recording_stereo()` - stereo initial (stereo_recording.hpp)

**The pattern:**
```cpp
if (recordhead == playhead) {
    writeval += recin;  // accumulate
    pokesteps += 1.0;
} else {
    if (pokesteps > 1.0) {
        writeval /= pokesteps;  // average for slow speed
    }
    b[recordhead] = writeval;

    // Linear interpolation for fast speed
    if (recplaydif > 0) {
        coeff = (recin - writeval) / recplaydif;
        for (i = recordhead+1; i < playhead; i++) {
            writeval += coeff;
            b[i] = writeval;
        }
    }
}
```

**Differences:**
1. Regular vs Initial: Initial has wrap-around logic for direction changes
2. Mono vs Stereo: Just processes 1 vs 2 channels

**Unification strategy:**
```cpp
template<int CHANNELS, bool IS_INITIAL>
void process_ipoke_recording_unified(
    float* b, long pchans, long playhead, long* recordhead,
    double* recin,  // array of CHANNELS samples
    // ... other params
) {
    for (int c = 0; c < CHANNELS; c++) {
        // Shared iPoke logic
        if (*recordhead == playhead) {
            writeval[c] += recin[c];
            // ...
        } else {
            // ...
            if constexpr (IS_INITIAL) {
                // Wrap-around handling for initial loop
            } else {
                // Simple interpolation
            }
        }
    }
}
```

**Benefits:**
- **Reduce ~600 lines to ~150 lines** (75% reduction)
- Single place to fix bugs
- Template ensures zero overhead

---

### 3. Eliminate kh_* Wrapper Layer (LOW EFFORT, MEDIUM IMPACT)

**Current situation:**
```cpp
// Forward declaration
static inline void kh_process_ipoke_recording(...);

// Implementation (just calls header)
static inline void kh_process_ipoke_recording(...) {
    karma::process_ipoke_recording(...);
}

// Usage
kh_process_ipoke_recording(...);
```

**This is completely pointless.** Just call `karma::process_ipoke_recording()` directly.

**Strategy:**
1. Global find/replace: `kh_process_` → `karma::process_`
2. Global find/replace: `kh_calculate_` → `karma::calculate_`
3. Global find/replace: `kh_handle_` → `karma::handle_`
4. Delete all the kh_* wrapper implementations
5. Delete all the kh_* forward declarations

**Benefits:**
- **Eliminate ~200 lines** of pointless wrappers
- Reduce indirection
- Clearer call hierarchy

---

### 4. Consolidate State Machine (LOW IMPACT)

**Current:** 11-state `control_state_t` enum processed in massive switch statement

**Analysis:** The states are actually necessary for precise DSP timing. Each state represents a distinct fade/ramp behavior that can't be collapsed.

**Conclusion:** Leave this alone. The complexity is inherent to the problem domain.

---

### 5. Reduce Function Parameter Counts (MEDIUM EFFORT, LOW IMPACT)

**Current problem:**
```cpp
static inline double kh_process_ramps_and_fades(
    double osamp1, double* o1prev, double* o1dif, double* snrfade,
    long* playfade, double globalramp, double snrramp,
    switchramp_type_t snrtype, char* playfadeflag, t_bool* go,
    t_bool* triginit, t_bool* jumpflag, t_bool* loopdetermine,
    t_bool record) noexcept
```

**14 parameters!** This happens when you extract without refactoring.

**Strategy A:** Create parameter structs
```cpp
struct FadeState {
    double* o1prev, *o1dif, *snrfade;
    long* playfade;
    char* playfadeflag;
};

struct FadeConfig {
    double globalramp, snrramp;
    switchramp_type_t snrtype;
};

double process_ramps_and_fades(
    double osamp, FadeState& state, const FadeConfig& config,
    PlaybackFlags& flags, bool record);
```

**Strategy B:** Just pass the t_karma* pointer
```cpp
double process_ramps_and_fades(t_karma* x, double osamp, bool record) {
    // Access x->fade.*, x->state.* directly
}
```

**Trade-off:**
- Strategy A: More typing, clearer dependencies
- Strategy B: Simpler, but tighter coupling

---

## Recommended Implementation Order

### Phase 1: Low-Hanging Fruit (1-2 hours)
1. **Eliminate kh_* wrappers** - Global find/replace, immediate cleanup
2. **Remove redundant headers** - Some headers are tiny and could be merged

**Expected savings: ~250 lines**

### Phase 2: Major Unification (4-6 hours)
3. **Unify perform functions** using templates
   - Start with if constexpr for channel-specific sections
   - Extract common logic first
   - Test thoroughly in Max after each change

**Expected savings: ~700 lines**

### Phase 3: Deep Refactoring (6-8 hours)
4. **Unify iPoke recording** variants
5. **Reduce parameter counts** where egregious

**Expected savings: ~400 lines**

---

## Expected Final State

**Current:**
- k4~.cpp: 2,503 lines
- Headers: ~2,000 lines
- **Total: ~4,500 lines**

**After simplification:**
- k4~.cpp: ~1,800 lines (unified perform functions)
- Headers: ~1,200 lines (unified iPoke, removed wrappers)
- **Total: ~3,000 lines (33% reduction)**

**Plus qualitative improvements:**
- Changes only need to be made once
- Bugs fixed in one place
- Easier to understand (less fragmentation)
- Better test surface area

---

## Risk Assessment

**Low Risk:**
- Removing kh_* wrappers (mechanical change)
- Consolidating headers (organizational)

**Medium Risk:**
- Unifying perform functions (complex but well-isolated)
- Template approach compiles to same code

**High Risk:**
- Changing state machine logic (would break DSP)
- Modifying iPoke algorithms (subtle math)

**Mitigation:**
- Commit after each step
- Test in Max/MSP extensively
- Compare audio output before/after
- Keep original code in separate branch

---

## Alternative: Stop Refactoring Entirely

**Honest assessment:** The code works. It's 4,500 lines to implement a complex looper with:
- 3 channel modes (mono/stereo/poly)
- 11 internal states
- Variable-speed recording with interpolation
- Direction changes during recording
- Loop windowing
- Overdubbing with fades
- Jump/wrap modes

**That's not actually that much code for what it does.**

The real question: **What problem are we trying to solve?**
- Maintainability? → Unify the perform functions
- Binary size? → Already optimal at 286K
- Performance? → Already fast (DSP critical path)
- Readability? → The fragmentation made it worse

**Recommendation:** Do Phase 1 (remove wrappers) and Phase 2 (unify performs), then **stop**.
