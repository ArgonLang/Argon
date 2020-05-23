cmake_minimum_required(VERSION 3.7)

add_library(object SHARED integer.cc decimal.cc string.cc code.cc list.cc tuple.cc map.cc hash_magic.cc nil.cc bool.cc function.cc bounds.cc module.cc error.cc refcount.cc)
add_dependencies(object memory)
target_link_libraries(object memory)