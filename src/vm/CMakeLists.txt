cmake_minimum_required(VERSION 3.7)

add_library(avm SHARED argon_vm.cc)
add_dependencies(avm memory object)
target_link_libraries(avm memory object)