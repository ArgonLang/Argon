    include(FetchContent)

    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test "*.cpp")
    message(STATUS "Test case source files: ${TEST_SRC_FILES}")

    add_executable(ArgonTest ${TEST_SRC_FILES})
    add_dependencies(ArgonTest Stratum)

    target_link_libraries(ArgonTest ArgonVM GTest::gtest_main)

    include(GoogleTest)

    enable_testing()
    gtest_discover_tests(ArgonTest)