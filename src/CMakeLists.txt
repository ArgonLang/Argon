cmake_minimum_required(VERSION 3.7)

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory)

add_subdirectory(lang)
add_subdirectory(memory)
add_subdirectory(module)
add_subdirectory(object)
add_subdirectory(vm)