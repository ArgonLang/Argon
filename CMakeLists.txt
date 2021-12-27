cmake_minimum_required(VERSION 3.7)
project(Argon)

set(AR_MAJOR 0)
set(AR_MINOR 2)
set(AR_PATCH 1)
set(AR_RELEASE_LEVEL "alpha")

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin")
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

if (${CMAKE_CXX_COMPILER_ID} MATCHES "^(Apple)?Clang$" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -pthread")
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include_directories(src)

add_subdirectory(src)
#add_subdirectory(test)
