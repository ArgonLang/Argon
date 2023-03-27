configure_file("${CMAKE_CURRENT_SOURCE_DIR}/vm/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/vm/version.h")

add_library(ArgonVM SHARED)
add_dependencies(ArgonVM Stratum)

file(GLOB_RECURSE VM_SRC RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "*.cpp")
message(STATUS "ArgonVM source files: ${VM_SRC}")

target_sources(ArgonVM PRIVATE ${VM_SRC})
target_include_directories(ArgonVM PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ArgonVM PUBLIC Stratum)

if(WIN32)
    target_link_libraries(ArgonVM ws2_32)
endif()

if (NOT MSVC)
    target_compile_options(ArgonVM PRIVATE -fPIC)
endif ()

set_target_properties(ArgonVM PROPERTIES OUTPUT_NAME "argon")
set_target_properties(ArgonVM PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(ArgonVM PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})