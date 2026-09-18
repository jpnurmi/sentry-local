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

add_executable(app-hang src/app_hang.c)
target_include_directories(app-hang PRIVATE
    $<TARGET_PROPERTY:sentry,INCLUDE_DIRECTORIES>)
target_compile_definitions(app-hang PRIVATE
    $<TARGET_PROPERTY:sentry,COMPILE_DEFINITIONS>)
target_link_libraries(app-hang PRIVATE sentry)

add_executable(cpp-exception src/cpp_exception.cpp)
target_link_libraries(cpp-exception PRIVATE sentry)

if(APPLE)
    add_custom_command(TARGET app-hang POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:app-hang>"
        VERBATIM)
    add_custom_command(TARGET cpp-exception POST_BUILD
        COMMAND dsymutil "$<TARGET_FILE:cpp-exception>"
        VERBATIM)
endif()
