cmake_minimum_required(VERSION 3.7)

set(SRC builtins.cc runtime.cc io/iomodule.cc io/io.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
