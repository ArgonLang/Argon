cmake_minimum_required(VERSION 3.7)

set(SOCKET_SRC
        socket/skmodule.cc
        socket/socket.cc)

set(SSL_SRC
        ssl/ssl.cc
        ssl/sslcontext.cc
        ssl/sslsocket.cc)

set(SYNC_SRC
        sync/cond.cc
        sync/sync.cc
        sync/mutex.cc
        sync/rwmutex.cc
        sync/notifyqueue.cc
        sync/locker.cc)

set(SRC ${SOCKET_SRC}
        ${SSL_SRC}
        ${SYNC_SRC}
        builtins.cc
        error.cc
        gcmodule.cc
        iomodule.cc
        math.cc
        nt/nt.cc
        osmodule.cc
        random.cc
        regex.cc
        runtime.cc)

list(TRANSFORM SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
target_sources(argon PRIVATE ${SRC})
