cmake_minimum_required(VERSION 3.7)

add_library(object SHARED integer.cc decimal.cc string.cc code.cc list.cc)
add_dependencies(object memory)
target_link_libraries(object memory)