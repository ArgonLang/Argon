cmake_minimum_required(VERSION 3.15)

add_library(${PROJECT_NAME} SHARED
        arena.cpp
        memory.cpp
        memutil.cpp
        osmemory.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(
        ${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

if (NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif ()
