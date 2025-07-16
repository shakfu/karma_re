HOMEBREW := $(shell brew --prefix)
CLANG_TIDY := $(HOMEBREW)/opt/llvm/bin/clang-tidy
C74_INCLUDES := source/max-sdk-base/c74support
MAX_INCLUDES := $(C74_INCLUDES)/max-includes
MSP_INCLUDES := $(C74_INCLUDES)/msp-includes
HOMEBREW_INCLUDES := $(HOMEBREW)/include

define tidy-target
@$(CLANG_TIDY) '$1' -- \
	-I $(MAX_INCLUDES) -I $(MSP_INCLUDES) \
	-I $(HOMEBREW_INCLUDES)
endef


.phony: all build dev tidy clean

all: build

build: clean
	@mkdir build && \
		cd build && \
		cmake -GXcode .. \
			&& \
		cmake --build . --config Release

dev: clean
	@mkdir build && \
		cd build && \
		cmake .. \
			&& \
		cmake --build . --config Debug

tidy:
	$(call tidy-target,source/projects/karma_tilde/karma~.c)

clean:
	@rm -rf build externals

