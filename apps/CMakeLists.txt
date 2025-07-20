add_executable(play play.cc)
target_link_libraries(play PRIVATE mytikas)

add_executable(precalculate precalculate.cc)
target_link_libraries(precalculate PRIVATE mytikas)
