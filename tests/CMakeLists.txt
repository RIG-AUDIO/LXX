# Test executables
file(GLOB TEST_SOURCES "*.cpp")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    if(test_name MATCHES "^_")
        continue()
    endif()
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE lxx_dsp)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

if(LXX_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()

# dr_wav for WAV I/O (header-only, fetched if needed)
find_package(drwav QUIET)
if(NOT drwav_FOUND)
    message(STATUS "dr_wav not found, tests will use CSV output only")
endif()