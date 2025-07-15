file(GLOB HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")

add_library(lib state.cc ${HEADERS})

target_include_directories(lib PUBLIC ${PROJECT_SOURCE_DIR}/include)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
