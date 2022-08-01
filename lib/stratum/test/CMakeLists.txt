cmake_minimum_required(VERSION 3.15)

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

file(GLOB_RECURSE TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test "*.cpp")
message(STATUS "Test case source files: ${TEST_SRC_FILES}")

add_executable(StratumTest ${TEST_SRC_FILES})
#add_executable(StratumTest test_main.cpp min_heap.cpp queue.cpp memutil.cpp)
add_dependencies(StratumTest Stratum)
target_link_libraries(StratumTest Stratum gtest gtest_main)
