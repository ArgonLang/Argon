cmake_minimum_required(VERSION 3.7)

set(SRC
        datatype/option.cc
        datatype/iterator.cc
        datatype/set.cc
        datatype/bytes.cc
        datatype/bytestream.cc
        datatype/hmap.cc
        datatype/integer.cc
        datatype/decimal.cc
        datatype/string.cc
        datatype/code.cc
        datatype/list.cc
        datatype/tuple.cc
        datatype/map.cc
        datatype/hash_magic.cc
        datatype/nil.cc
        datatype/bool.cc
        datatype/function.cc
        datatype/bounds.cc
        datatype/module.cc
        datatype/error.cc
        refcount.cc
        datatype/namespace.cc
        datatype/struct.cc
        datatype/trait.cc
        datatype/instance.cc
        gc.cc
        arobject.cc
        setup.cc
        datatype/support/bytesops.cc)

list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
