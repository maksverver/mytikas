file(GLOB HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")
file(GLOB SOURCES "*.cc")

add_library(mytikas ${SOURCES} ${HEADERS})

target_include_directories(mytikas PUBLIC ${PROJECT_SOURCE_DIR}/include)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
