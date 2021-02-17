cmake_minimum_required(VERSION 3.7)

project(ArgonTests)

# Download and unpack googletest at configure time
configure_file(GTestDownloader.txt GTest/CMakeLists.txt)

execute_process(
        COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GTest
)

if (result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif ()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GTest)

if (result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif ()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(
        ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
        ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL
)

file(GLOB_RECURSE TEST_SRC_FILES RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "*.cc")
message(STATUS "Test case source files: ${TEST_SRC_FILES}")

add_executable(ArgonTests ${TEST_SRC_FILES})
target_link_libraries(ArgonTests gtest gtest_main)

add_dependencies(ArgonTests argon memory)
target_link_libraries(ArgonTests argon memory)