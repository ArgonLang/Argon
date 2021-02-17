cmake_minimum_required(VERSION 3.7)

set(SRC argon.cc areval.cc frame.cc ar_routine.cc routine_queue.cc runtime.cc context.cc import.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
