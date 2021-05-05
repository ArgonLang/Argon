cmake_minimum_required(VERSION 3.7)

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory)

add_executable(main main.cc)
target_link_libraries(main argon)
add_dependencies(main argon)
set_target_properties(main PROPERTIES OUTPUT_NAME "argon")

file(COPY arlib/ DESTINATION ${LIBRARY_OUTPUT_PATH}/)

add_subdirectory(lang)
add_subdirectory(memory)
add_subdirectory(module)
add_subdirectory(object)
add_subdirectory(vm)