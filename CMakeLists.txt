cmake_minimum_required(VERSION 3.31)
project(mytikas VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall)

if ( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
     CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    add_compile_options(-Wextra -Wno-sign-compare)
endif()

include(CTest)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
)
FetchContent_MakeAvailable(googletest)

include_directories(include)
add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)
