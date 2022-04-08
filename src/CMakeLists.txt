cmake_minimum_required(VERSION 3.7)

find_package(OpenSSL REQUIRED)

add_library(argon SHARED)
add_dependencies(argon memory)
target_link_libraries(argon memory OpenSSL::SSL)

if (MSVC)
    target_link_libraries(argon crypt32.lib)
endif ()

add_executable(main main.cc)
target_link_libraries(main argon)
add_dependencies(main argon)
set_target_properties(main PROPERTIES OUTPUT_NAME "argon")

file(COPY arlib/ DESTINATION ${LIBRARY_OUTPUT_PATH}/packages)

add_subdirectory(lang)
add_subdirectory(memory)
add_subdirectory(module)
add_subdirectory(object)
add_subdirectory(vm)