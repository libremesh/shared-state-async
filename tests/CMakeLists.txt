cmake_minimum_required(VERSION 3.14)

# Doctest.cmake lives in tests/cmake/, not in a top-level cmake/ dir
# (which does not exist), so CMAKE_SOURCE_DIR pointed at nothing and
# include(Doctest) failed. This suite therefore never configured while
# SS_TESTS defaulted to OFF and nobody turned it on.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

include(CTest)

# Doctest.cmake only fetches doctest inside `if(ENABLE_DOCTESTS)`, and
# nothing ever set that variable — so the module was a no-op and the
# tests failed to compile on a missing doctest/doctest.h. This file is
# only added to the build when SS_TESTS is ON, so enabling it here is
# equivalent to "the user asked for tests".
set(ENABLE_DOCTESTS ON)
include(Doctest)

# List all files containing tests. (Change as needed)
set(TESTFILES        # All .cpp files in tests/
    main.cpp
    bleach_tests.cc
    state_serialization_tests.cc
)

set(TEST_MAIN unit_tests)   # Default name for test executable (change if you wish).
set(TEST_RUNNER_PARAMS "-s")  # Any arguemnts to feed the test runner (change as needed).

# --------------------------------------------------------------------------------
#                         Make Tests (no change needed).
# --------------------------------------------------------------------------------
add_executable(${TEST_MAIN} ${TESTFILES})
target_link_libraries(${TEST_MAIN} PRIVATE ${LIBRARY_NAME} doctest)
set_target_properties(${TEST_MAIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
# target_set_warnings() comes from a Warnings.cmake module that was
# never vendored into this repository, so calling it unconditionally is
# a hard configure error. Guarded so the suite builds either way.
if(COMMAND target_set_warnings)
    target_set_warnings(${TEST_MAIN} ENABLE ALL AS_ERROR ALL DISABLE Annoying)
endif()

set_target_properties(${TEST_MAIN} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_test(
    # Use some per-module/project prefix so that it is easier to run only tests for this module
    NAME ${LIBRARY_NAME}.${TEST_MAIN}
    COMMAND ${TEST_MAIN} ${TEST_RUNNER_PARAMS})

# Adds a 'coverage' target. CodeCoverage.cmake was never vendored into
# this repository either, so this is OPTIONAL: absent it, the suite still
# builds and runs, only the 'coverage' target is unavailable.
include(CodeCoverage OPTIONAL RESULT_VARIABLE SS_CODECOVERAGE_FOUND)
