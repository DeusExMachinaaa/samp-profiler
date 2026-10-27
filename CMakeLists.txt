cmake_minimum_required(VERSION 3.18)
project(samp_profiler LANGUAGES CXX)

option(SAMP_PROFILER_BUILD_TESTS "Build standalone profiler tests" ON)
set(SAMP_PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source/vendor/samp-plugin-sdk"
    CACHE PATH "Path to the SA-MP Plugin SDK")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(samp-profiler SHARED
    source/src/plugin.cpp
    source/src/profiler_core.cpp
    ${SAMP_PLUGIN_SDK_DIR}/amxplugin.cpp
)

target_include_directories(samp-profiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source/src
    ${SAMP_PLUGIN_SDK_DIR}
)

if (WIN32)
    target_compile_definitions(samp-profiler PRIVATE WIN32 HAVE_STDINT_H)
    target_compile_options(samp-profiler PRIVATE /W4 /EHsc)
    set_target_properties(samp-profiler PROPERTIES PREFIX "")
else()
    target_compile_definitions(samp-profiler PRIVATE LINUX SAMPSVR HAVE_STDINT_H)
    target_compile_options(samp-profiler PRIVATE -fvisibility=hidden -Wall -Wextra -Wpedantic)
    set_target_properties(samp-profiler PROPERTIES PREFIX "")
endif()

if (SAMP_PROFILER_BUILD_TESTS)
    enable_testing()
    add_executable(samp-profiler-tests
        tests/profiler_core_test.cpp
        source/src/profiler_core.cpp
    )
    target_include_directories(samp-profiler-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source/src)
    if (WIN32)
        target_compile_definitions(samp-profiler-tests PRIVATE WIN32 HAVE_STDINT_H)
    else()
        target_compile_definitions(samp-profiler-tests PRIVATE LINUX SAMPSVR HAVE_STDINT_H)
    endif()
    add_test(NAME samp-profiler-tests COMMAND samp-profiler-tests)
endif()
