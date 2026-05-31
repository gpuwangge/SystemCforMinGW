file(GLOB COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)
add_library(common STATIC ${COMMON_SOURCES})
target_include_directories(common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
