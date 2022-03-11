cmake_minimum_required(VERSION 3.7)

configure_file("version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/version.h")

set(SYNC_SRC
        sync/mutex.cc
        sync/queue.cc
        sync/ticketqueue.cc)

set(SRC ${SYNC_SRC}
        areval.cc
        argon.cc
        arroutine.cc
        callhelper.cc
        config.cc
        context.cc
        import.cc
        routinequeue.cc
        runtime.cc)

list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
