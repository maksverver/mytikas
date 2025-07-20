add_executable(moves_test moves_test.cc)
target_link_libraries(moves_test mytikas GTest::gtest_main GTest::gmock)
add_test(NAME moves_test COMMAND moves_test)

include(GoogleTest)
gtest_discover_tests(moves_test)
