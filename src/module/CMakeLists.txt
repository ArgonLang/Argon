cmake_minimum_required(VERSION 3.7)

set(SRC builtins.cc error.cc gcmodule.cc iomodule.cc math.cc osmodule.cc random.cc runtime.cc  nt/nt.cc sync/sync.cc sync/mutex.cc sync/notifyqueue.cc sync/locker.cc)
list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
