cmake_minimum_required(VERSION 3.7)

configure_file("version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/version.h")

set(SRC argon.cc areval.cc callhelper.cc frame.cc arroutine.cc notifyqueue.cc routinequeue.cc runtime.cc config.cc context.cc import.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
