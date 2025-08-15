add_executable(play play.cc)
target_link_libraries(play PRIVATE mytikas)

add_executable(precalculate precalculate.cc)
target_link_libraries(precalculate PRIVATE mytikas)

if (DEFINED EMSCRIPTEN)
add_executable(wasm-api wasm-api.cc)
target_link_libraries(wasm-api PRIVATE mytikas)
target_link_options(wasm-api PRIVATE "-sEXPORTED_FUNCTIONS=getValue,setValue" "-sEXPORTED_RUNTIME_METHODS=UTF8ToString")
endif()
