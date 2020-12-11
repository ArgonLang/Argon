cmake_minimum_required(VERSION 3.7)

add_library(modules SHARED builtins.cc runtime.cc io/iomodule.cc io/io.cc)
add_dependencies(modules object)
target_link_libraries(modules object)
