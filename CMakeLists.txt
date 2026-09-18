cmake_minimum_required(VERSION 3.18)
project(sentry-local LANGUAGES C CXX)

set(SENTRY_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sentry-native" CACHE PATH
    "Path to the sentry-native checkout to test")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(SENTRY_BACKEND native CACHE STRING "" FORCE)
set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SENTRY_INTEGRATION_CPP ON CACHE BOOL "" FORCE)
add_subdirectory("${SENTRY_NATIVE_DIR}" sdk)

add_executable(hang src/hang.c)
target_include_directories(hang PRIVATE
    $<TARGET_PROPERTY:sentry,INCLUDE_DIRECTORIES>)
target_compile_definitions(hang PRIVATE
    $<TARGET_PROPERTY:sentry,COMPILE_DEFINITIONS>)
target_link_libraries(hang PRIVATE sentry)

add_executable(cpp_exception src/cpp_exception.cpp)
target_link_libraries(cpp_exception PRIVATE sentry)

if(APPLE)
    add_custom_command(TARGET hang POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:hang>"
        VERBATIM)
    add_custom_command(TARGET cpp_exception POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:cpp_exception>"
        VERBATIM)
endif()
