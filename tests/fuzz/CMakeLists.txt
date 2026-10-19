# libFuzzer harness for lxx_dsp.  Clang only; never built by default and
# never registered with CTest (it is driven directly by the fuzz workflow).

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LXX_BUILD_FUZZ requires the Clang toolchain")
endif()

add_executable(lxx_dsp_fuzz dsp_fuzz.cpp)

target_link_libraries(lxx_dsp_fuzz PRIVATE lxx_dsp)

target_compile_options(lxx_dsp_fuzz PRIVATE
    -fsanitize=fuzzer,address
    -fno-omit-frame-pointer
    -g
    -O1
)
target_link_options(lxx_dsp_fuzz PRIVATE
    -fsanitize=fuzzer,address
)
