cmake_minimum_required(VERSION 3.7)

option(ARGON_NO_SSL "Build Argon without SSL support")

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory)

if (NOT ARGON_NO_SSL)
    add_dependencies(argon ssl)
    target_link_libraries(argon ssl)
else ()
    target_compile_definitions(argon PRIVATE _ARGON_NO_SSL)
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