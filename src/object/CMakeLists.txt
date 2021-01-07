cmake_minimum_required(VERSION 3.7)

add_library(object SHARED datatype/hmap.cc datatype/set.cc datatype/integer.cc datatype/decimal.cc datatype/string.cc datatype/code.cc datatype/list.cc datatype/tuple.cc datatype/map.cc datatype/hash_magic.cc datatype/nil.cc datatype/bool.cc datatype/function.cc datatype/bounds.cc datatype/module.cc datatype/error.cc refcount.cc datatype/namespace.cc datatype/struct.cc datatype/trait.cc datatype/instance.cc gc.cc arobject.cc datatype/support/bytesops.cc)
add_dependencies(object memory)
target_link_libraries(object memory)