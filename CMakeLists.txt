cmake_minimum_required(VERSION 3.15)
project(Argon VERSION 1.0.0)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -pthread")
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include_directories(src)
include_directories(lib)

add_subdirectory(lib/stratum)
add_subdirectory(src)
add_subdirectory(test)

file(COPY arlib/ DESTINATION ${LIBRARY_OUTPUT_PATH}/packages)
