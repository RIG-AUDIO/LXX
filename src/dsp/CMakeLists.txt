# DSP static library
file(GLOB_RECURSE DSP_SOURCES
    "core/*.cpp"
    "modulation/*.cpp"
    "envelopes/*.cpp"
    "lfo/*.cpp"
    "oscillators/*.cpp"
    "filters/*.cpp"
    "amplifiers/*.cpp"
    "drums/*.cpp"
    "voice/*.cpp"
    "sequencer/*.cpp"
    "sequencer/grids/*.cpp"
    "engines/*.cpp"
    "*.cpp"
)

add_library(lxx_dsp STATIC ${DSP_SOURCES})

# The DSP library is linked into the VST3 shared module, so every object must
# be position independent.  Without this the plugin link fails with
# "relocation R_X86_64_PC32 ... can not be used when making a shared object".
set_target_properties(lxx_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(lxx_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/modulation
        ${CMAKE_CURRENT_SOURCE_DIR}/envelopes
        ${CMAKE_CURRENT_SOURCE_DIR}/lfo
        ${CMAKE_CURRENT_SOURCE_DIR}/oscillators
        ${CMAKE_CURRENT_SOURCE_DIR}/filters
        ${CMAKE_CURRENT_SOURCE_DIR}/amplifiers
        ${CMAKE_CURRENT_SOURCE_DIR}/drums
        ${CMAKE_CURRENT_SOURCE_DIR}/voice
        ${CMAKE_CURRENT_SOURCE_DIR}/sequencer
        ${CMAKE_CURRENT_SOURCE_DIR}/engines
)

if(MSVC)
    target_compile_options(lxx_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(lxx_dsp PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()
lxx_werror(lxx_dsp)

# Install headers
file(GLOB_RECURSE DSP_HEADERS
    "core/*.h"
    "modulation/*.h"
    "envelopes/*.h"
    "lfo/*.h"
    "oscillators/*.h"
    "filters/*.h"
    "amplifiers/*.h"
    "drums/*.h"
    "voice/*.h"
    "sequencer/*.h"
    "engines/*.h"
)

install(FILES ${DSP_HEADERS} DESTINATION include/lxx/dsp)
install(TARGETS lxx_dsp DESTINATION lib)
