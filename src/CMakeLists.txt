cmake_minimum_required(VERSION 3.7)

option(ARGON_FF_SSL "Build Argon with SSL support" on)

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory)

if (ARGON_FF_SSL)
    add_dependencies(argon ssl)
    target_link_libraries(argon ssl)
    target_compile_definitions(argon PRIVATE ARGON_FF_SSL)
endif ()

add_executable(main main.cc)
add_dependencies(main argon)
target_link_libraries(main argon)
set_target_properties(main PROPERTIES OUTPUT_NAME "argon")

file(COPY arlib/ DESTINATION ${LIBRARY_OUTPUT_PATH}/packages)

add_subdirectory(lang)
add_subdirectory(memory)
add_subdirectory(module)
add_subdirectory(object)
add_subdirectory(vm)