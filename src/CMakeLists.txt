add_executable(Argon main.cc)
add_dependencies(Argon ArVM)
target_link_libraries(Argon ArVM)

add_subdirectory(vm)