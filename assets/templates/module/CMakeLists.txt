cmake_minimum_required(VERSION 3.5)
project({{MODULE_NAME}} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

if(NOT MODULE_SOURCES)
    message(FATAL_ERROR "No source files found in ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_library({{MODULE_NAME}} SHARED ${MODULE_SOURCES})

target_include_directories({{MODULE_NAME}} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties({{MODULE_NAME}} PROPERTIES OUTPUT_NAME "{{MODULE_NAME}}")

if(UNIX)
    set_target_properties({{MODULE_NAME}} PROPERTIES PREFIX "")
endif()

if(WIN32)
    set_target_properties({{MODULE_NAME}} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
