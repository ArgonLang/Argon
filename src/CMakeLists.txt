cmake_minimum_required(VERSION 3.7)

option(ARGON_FF_CGOTO "Compile Argon using the computed goto extension" on)
option(ARGON_FF_SSL "Build Argon with SSL support" on)
option(ARGON_FF_UNL "Compile Argon using the universal newline support" on)

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory)

if(NOT MSVC AND ARGON_FF_CGOTO)
    target_compile_definitions(argon PRIVATE ARGON_FF_COMPUTED_GOTO)
endif()

if (ARGON_FF_SSL)
    add_dependencies(argon ssl)
    target_link_libraries(argon ssl)
    target_compile_definitions(argon PRIVATE ARGON_FF_SSL)
endif ()

if(ARGON_FF_UNL)
    target_compile_definitions(argon PRIVATE ARGON_FF_UNIVERSAL_NEWLINE)
endif()

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