cmake_minimum_required(VERSION 3.7)

set(SRC builtins.cc error.cc iomodule.cc math.cc random.cc runtime.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
