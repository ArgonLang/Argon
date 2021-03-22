cmake_minimum_required(VERSION 3.7)
project(Argon)

set(AR_MAJOR 0)
set(AR_MINOR 1)
set(AR_PATCH 0)
set(AR_RELEASE_LEVEL "alpha")
set(AR_VERSION "0.1.0-alpha")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

include_directories(src)

add_subdirectory(src)
add_subdirectory(test)