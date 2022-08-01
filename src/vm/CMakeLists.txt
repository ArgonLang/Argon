file(GLOB_RECURSE VM_SRC RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "*.cpp")
message(STATUS "ArgonVM source files: ${VM_SRC}")

add_library(ArVM SHARED ${VM_SRC})
add_dependencies(ArVM Stratum)
target_link_libraries(ArVM Stratum)

if(NOT MSVC)
    target_compile_options(ArVM PRIVATE -fPIC)
endif()

set_target_properties(ArVM PROPERTIES OUTPUT_NAME "argon")
