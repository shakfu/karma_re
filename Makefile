PKG_NAME := "karma"
ROOTDIR := $(shell pwd)
HOMEBREW := $(shell brew --prefix)
CLANG_TIDY := $(HOMEBREW)/opt/llvm/bin/clang-tidy
C74_INCLUDES := source/max-sdk-base/c74support
MAX_INCLUDES := $(C74_INCLUDES)/max-includes
MSP_INCLUDES := $(C74_INCLUDES)/msp-includes
HOMEBREW_INCLUDES := $(HOMEBREW)/include
MAX_VERSIONS := 8 9

define tidy-target
@$(CLANG_TIDY) '$1' -- \
	-I $(MAX_INCLUDES) -I $(MSP_INCLUDES) \
	-I $(HOMEBREW_INCLUDES)
endef


.phony: all build dev format tidy complexity link clean

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

format:
	@clang-format -i source/projects/karma_tilde/karma\~.cpp

tidy:
	$(call tidy-target,source/projects/karma_tilde/karma~.cpp)


complexity:
	@complexity source/projects/karma_tilde/karma\~.cpp

link:
	$(call section,"symlink to Max 'Packages' Directories")
	@for MAX_VERSION in $(MAX_VERSIONS); do \
		MAX_DIR="Max $${MAX_VERSION}" ; \
		PACKAGES="$(HOME)/Documents/$${MAX_DIR}/Packages" ; \
		THIS_PACKAGE="$${PACKAGES}/$(PKG_NAME)" ; \
		if [ -d "$${PACKAGES}" ]; then \
			echo "symlinking to $${THIS_PACKAGE}" ; \
			if ! [ -L "$${THIS_PACKAGE}" ]; then \
				ln -s "$(ROOTDIR)" "$${THIS_PACKAGE}" ; \
				echo "... symlink created" ; \
			else \
				echo "... symlink already exists" ; \
			fi \
		fi \
	done

cpd:
	@pmd cpd --minimum-tokens 50 --language cpp source/projects/karma_tilde/karma\~.cpp


clean:
	@rm -rf build externals


