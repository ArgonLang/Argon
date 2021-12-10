cmake_minimum_required(VERSION 3.7)

set(SRC
        arobject.cc
        bufview.cc
        datatype/bool.cc
        datatype/bounds.cc
        datatype/bytes.cc
        datatype/code.cc
        datatype/decimal.cc
        datatype/error.cc
        datatype/function.cc
        datatype/hash_magic.cc
        datatype/hmap.cc
        datatype/io/io.cc
        datatype/io/io_base.cc
        datatype/integer.cc
        datatype/iterator.cc
        datatype/list.cc
        datatype/map.cc
        datatype/module.cc
        datatype/namespace.cc
        datatype/nativewrap.cc
        datatype/nil.cc
        datatype/option.cc
        datatype/set.cc
        datatype/string.cc
        datatype/struct.cc
        datatype/support/bytesops.cc
        datatype/tuple.cc
        gc.cc
        refcount.cc
        rwlock.cc
        setup.cc
)

list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
