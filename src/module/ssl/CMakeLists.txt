cmake_minimum_required(VERSION 3.7)

if (ARGON_FF_SSL)
    find_package(OpenSSL REQUIRED)

    add_library(ssl STATIC ssl.cc sslcontext.cc sslsocket.cc sslwin.cc)
    add_dependencies(ssl memory)
    target_link_libraries(ssl memory OpenSSL::SSL)
    target_compile_options(ssl PRIVATE -fPIC)

    if (MSVC)
        target_link_libraries(ssl crypt32.lib)
    endif ()
endif ()