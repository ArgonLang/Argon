configure_file("version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/version.h")

file(GLOB_RECURSE VM_SRC RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "*.cpp")
message(STATUS "ArgonVM source files: ${VM_SRC}")

list(TRANSFORM VM_SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(ArgonVM PRIVATE ${VM_SRC})
