cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Test executable
add_executable(tx_tests
    test_parser.cpp
    test_screen.cpp
    test_pty.cpp
)

target_link_libraries(tx_tests PRIVATE
    tx_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(tx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add tests
include(GoogleTest)
gtest_discover_tests(tx_tests)
