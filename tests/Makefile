# Offline test harness for karma~ DSP.
#
# Drives implementations outside of Max against a mock buffer~ (max_stub.c) and
# diffs their per-scenario output + final buffer sample-for-sample.

ROOT      := ..
SDK       := $(ROOT)/source/max-sdk-base/c74support
MAXINC    := $(SDK)/max-includes
MSPINC    := $(SDK)/msp-includes
HBINC     := $(shell brew --prefix)/include
REFDIR    := $(ROOT)/source/projects/karma_tilde
K4DIR     := $(ROOT)/source/projects/k4_tilde
COREDIR   := $(ROOT)/source/projects/karma_core

INCLUDES  := -I$(MAXINC) -I$(MSPINC) -I$(HBINC) -I.
CFLAGS    := -g -O2 -Wall -Wno-cast-function-type-mismatch
LDFLAGS   := -lm
BUILD     := build

.PHONY: all check diff unit core k4diff oracle k4 difftool clean
all: check

# Full check: extraction equivalence + kernel unit tests.
check: diff unit

# Primary check: extracted core must match the reference sample-for-sample.
diff: $(BUILD)/oracle $(BUILD)/core $(BUILD)/difftool
	@cd $(BUILD) >/dev/null; ./oracle >/dev/null; ./core >/dev/null
	@echo "=== ref vs core (sample-exact) ==="
	@cd $(BUILD) >/dev/null; ok=1; \
	for f in ref_*.bin; do s=$${f#ref_}; n=$${s%.bin}; \
	  echo "[$$n]"; ./difftool ref_$$n.bin core_$$n.bin || ok=0; \
	done; \
	if [ $$ok -eq 1 ]; then echo "CORE MATCHES REFERENCE"; else echo "DIVERGENCES FOUND"; fi

# Secondary: characterise how far k4 diverges from the reference.
k4diff: $(BUILD)/oracle $(BUILD)/k4 $(BUILD)/difftool
	@cd $(BUILD) >/dev/null; ./oracle >/dev/null; ./k4 >/dev/null
	@echo "=== ref vs k4 (sample-exact) ==="
	@cd $(BUILD) >/dev/null; \
	for f in ref_*.bin; do s=$${f#ref_}; n=$${s%.bin}; \
	  echo "[$$n]"; ./difftool ref_$$n.bin k4_$$n.bin || true; \
	done

# Kernel unit tests (pure interp / ease / ipoke / wrap math).
unit: $(BUILD)/unit
	@echo "=== kernel unit tests ==="; cd $(BUILD) && ./unit

oracle: $(BUILD)/oracle ; @cd $(BUILD) && ./oracle
core:   $(BUILD)/core   ; @cd $(BUILD) && ./core
k4:     $(BUILD)/k4     ; @cd $(BUILD) && ./k4

$(BUILD)/unit: unit_kernels.c $(COREDIR)/karma_core.c $(COREDIR)/karma_core.h | $(BUILD)
	@clang $(CFLAGS) $(INCLUDES) -I$(COREDIR) unit_kernels.c $(LDFLAGS) -o $@

$(BUILD)/oracle: oracle_main.c max_stub.c max_stub.h scenarios.h | $(BUILD)
	@clang $(CFLAGS) $(INCLUDES) -I$(REFDIR) oracle_main.c max_stub.c $(LDFLAGS) -o $@

$(BUILD)/core: core_main.c $(COREDIR)/karma_core.c $(COREDIR)/karma_core.h max_stub.c max_stub.h scenarios.h | $(BUILD)
	@clang $(CFLAGS) $(INCLUDES) -I$(COREDIR) core_main.c $(COREDIR)/karma_core.c max_stub.c $(LDFLAGS) -o $@

$(BUILD)/k4: k4_main.c max_stub.c max_stub.h scenarios.h | $(BUILD)
	@clang $(CFLAGS) $(INCLUDES) -I$(K4DIR) k4_main.c max_stub.c $(LDFLAGS) -o $@

$(BUILD)/difftool: diff.c | $(BUILD)
	@clang $(CFLAGS) diff.c -o $@

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	@rm -rf $(BUILD)
