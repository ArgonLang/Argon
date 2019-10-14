cmake_minimum_required(VERSION 3.7)
project(Argon)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(test)