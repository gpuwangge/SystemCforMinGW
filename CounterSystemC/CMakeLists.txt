cmake_minimum_required(VERSION 3.20)
SET(CMAKE_C_COMPILER "C:/mingw64/bin/gcc.exe")
SET(CMAKE_CXX_COMPILER "C:/mingw64/bin/g++.exe")
project(CounterSystemC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
message(STATUS "CMake version = ${CMAKE_VERSION}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) #all lib goes here(.a/.lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) #this is not required in windows (.so)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) #this is required in Windows (windows .dll/.exe)

#aux_source_directory({PROJECT_SOURCE_DIR}/source APP_SOURCES) #A legacy alternative to add src files
file(GLOB APP_SOURCES
    "${PROJECT_SOURCE_DIR}/source/*.c"
    "${PROJECT_SOURCE_DIR}/source/*.cc"
    "${PROJECT_SOURCE_DIR}/source/*.cpp"
)
add_executable(${PROJECT_NAME} ${APP_SOURCES}) #add all files from SRC folder

#Set Common library
add_subdirectory(common)
target_link_libraries(${PROJECT_NAME} PRIVATE common)

#Set SystemC related (target_must be put after add_executable())
set(SYSTEMC_HOME "C:/systemc/3.0.2-mingw")
target_include_directories(${PROJECT_NAME} PRIVATE
    "${SYSTEMC_HOME}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "${SYSTEMC_HOME}/lib"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    systemc
    #winpthread #uncomment this if see pthread error
)


