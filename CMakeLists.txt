cmake_minimum_required(VERSION 3.22)

project(LXX VERSION 1.0.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Build mode.
#
# LXX is built by CI.  A plain `cmake -S . -B build` outside GitHub Actions is
# refused unless a human has explicitly authorised a local build with
# `-DLXX_BUILD_MODE=local`.  Local artifacts are not reproducible and must
# never be published.
# ---------------------------------------------------------------------------
set(LXX_BUILD_MODE "ci" CACHE STRING "ci (default) or local (explicit human authorisation)")
set_property(CACHE LXX_BUILD_MODE PROPERTY STRINGS ci local)

if(NOT LXX_BUILD_MODE STREQUAL "local")
    if(NOT "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
        message(FATAL_ERROR
            "LXX builds are CI-only by default (LXX_BUILD_MODE=ci).\n"
            "This configure was started outside GitHub Actions without an\n"
            "explicit local authorisation.\n"
            "Re-run with:  cmake -S . -B build -DLXX_BUILD_MODE=local\n"
            "Local artifacts must never be published.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LXX_BUILD_TESTS  "Build the lxx_dsp test executables" ON)
option(LXX_BUILD_PLUGIN "Build the LXX VST3 plugin"          ON)
option(LXX_ENABLE_ASAN  "AddressSanitizer build (never for shipping)" OFF)
option(LXX_BUILD_FUZZ   "Build the libFuzzer harness (Clang only)"     OFF)
option(LXX_WARNINGS_AS_ERRORS "Treat first-party warnings as errors"  OFF)

# lxx_werror(<target>) — apply -Werror to ONE first-party target.
#
# Per-target on purpose: JUCE, the VST3 SDK and every other third-party
# target are never passed through this function, so upstream warnings stay
# non-fatal while our own translation units are held to zero warnings.
# This follows the opt-in, per-target pattern of an external diagnostics
# module (no global CMAKE_CXX_FLAGS mutation).
function(lxx_werror target)
    if(NOT LXX_WARNINGS_AS_ERRORS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX)
    else()
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

if(LXX_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

add_subdirectory(src/dsp)

if(LXX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# The fuzz harness is driven by its own workflow with LXX_BUILD_TESTS=OFF, so
# it is added from here rather than from tests/ (which is not configured in
# that mode).  It is built only when explicitly requested.
if(LXX_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()

if(LXX_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.10
    )
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(src/plugin)
endif()
