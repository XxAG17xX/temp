# test/error_correction/

add_executable(
    error_correction_test 
    error_correction_test.cpp
)
target_link_libraries(
    error_correction_test
    error_correction
    utils
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(error_correction_test)