# Test executables
file(GLOB TEST_SOURCES "*.cpp")

# Wall-clock budgets (tests/test_time_budget.h) are calibrated for
# uninstrumented builds.  Sanitizer builds run the same workload an order of
# magnitude slower, so the budget — and only the budget — is scaled.  Sanity
# assertions (finiteness, amplitude, DC, counts) are never scaled.
if(NOT DEFINED LXX_TEST_TIME_SCALE)
    if(LXX_ENABLE_ASAN OR "${CMAKE_CXX_FLAGS}" MATCHES "-fsanitize")
        # Sanitizers: 10-50x slower than a normal run.
        set(LXX_TEST_TIME_SCALE 16.0)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # -O0 Debug: the budgets are calibrated on Release numbers, where the
        # realtime claim is enforced; a Debug run only has to prove "no hang".
        set(LXX_TEST_TIME_SCALE 16.0)
    else()
        set(LXX_TEST_TIME_SCALE 1.0)
    endif()
endif()

# CTest must never kill a test before its own in-test watchdog can report.
if(LXX_TEST_TIME_SCALE GREATER 1.0)
    set(LXX_TEST_TIMEOUT 5400)
else()
    set(LXX_TEST_TIMEOUT 900)
endif()

message(STATUS "LXX test time scale: ${LXX_TEST_TIME_SCALE} (timeout ${LXX_TEST_TIMEOUT}s)")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    if(test_name MATCHES "^_")
        continue()
    endif()
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE lxx_dsp)
    target_compile_definitions(${test_name} PRIVATE
        LXX_TEST_TIME_SCALE=${LXX_TEST_TIME_SCALE})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT ${LXX_TEST_TIMEOUT})
endforeach()

# dr_wav for WAV I/O (header-only, fetched if needed)
find_package(drwav QUIET)
if(NOT drwav_FOUND)
    message(STATUS "dr_wav not found, tests will use CSV output only")
endif()
