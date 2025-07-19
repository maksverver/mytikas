set(DOCTEST_DIR "${PROJECT_SOURCE_DIR}/vendor/doctest-2.4.12")

add_library(doctest INTERFACE)
target_compile_definitions(doctest INTERFACE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
target_include_directories(doctest INTERFACE "${DOCTEST_DIR}")
target_precompile_headers(doctest INTERFACE "${DOCTEST_DIR}/doctest.h")

add_executable(moves_test moves_test.cc)
target_link_libraries(moves_test mytikas doctest)
add_test(NAME moves_test COMMAND moves_test)
