include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

include_directories(
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
	"${CMAKE_CURRENT_SOURCE_DIR}/../karma_core"
)

# The thin Max shell plus the host-agnostic DSP core it wraps.
file(GLOB PROJECT_SRC
	"*.h"
	"*.c"
	"*.cpp"
)

add_library(
	${PROJECT_NAME}
	MODULE
	${PROJECT_SRC}
	"${CMAKE_CURRENT_SOURCE_DIR}/../karma_core/karma_core.c"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
