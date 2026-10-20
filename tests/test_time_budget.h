#pragma once

// Wall-clock budgets in tests.
//
// Several tests assert that a bounded workload finishes within a given wall
// time.  Those budgets are calibrated for uninstrumented builds; AddressSanitizer,
// UndefinedBehaviorSanitizer, Valgrind and friends make the identical workload
// an order of magnitude slower, which turns a "finishes in time" assertion into
// a false failure.
//
// LXX_TEST_TIME_SCALE is supplied by tests/CMakeLists.txt:
//   1.0  — normal build (GCC/Clang Debug and Release)
//   8+   — sanitizer / instrumentation builds
//
// Only wall-clock budgets go through this helper.  Correctness assertions —
// finiteness, NaN/Inf counts, DC, amplitude bounds, filter integrator clamps,
// timing accuracy — are never scaled.
#ifndef LXX_TEST_TIME_SCALE
#define LXX_TEST_TIME_SCALE 1.0
#endif

namespace lxx {
namespace test {

inline double timeBudget(double seconds)
{
    return seconds * static_cast<double>(LXX_TEST_TIME_SCALE);
}

} // namespace test
} // namespace lxx
