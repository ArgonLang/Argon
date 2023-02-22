// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <vm/io/io.h>

#include <vm/datatype/integer.h>

#include "socket.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

ARGON_FUNCTION(socket_socket, Socket,
               "",
               "i: family, i: type, i: protocol", false, false) {
    return (ArObject *) SocketNew((int) ((Integer *) args[0])->sint,
                                  (int) ((Integer *) args[1])->sint,
                                  (int) ((Integer *) args[2])->sint);
}

ARGON_METHOD(socket_connect, connect,
             "Connect to a remote socket at given address.\n"
             "\n"
             "- Parameter address: Format of address depends on the address family.\n",
             "st: address", false, false) {
    sockaddr_storage addr;
    socklen_t addrlen;

    auto *self = (Socket *) _self;

    if (!AddrToSockAddr(args[0], &addr, &addrlen, self->family))
        return nullptr;

    if (Connect(self, (const sockaddr *) &addr, addrlen) < 0)
        return nullptr;

    return IncRef(_self);
}

const FunctionDef sock_methods[] = {
        socket_socket,

        socket_connect,
        ARGON_METHOD_SENTINEL
};

TypeInfo *sock_bases[] = {
        (TypeInfo *) argon::vm::io::type_reader_t_,
        (TypeInfo *) argon::vm::io::type_writer_t_,
        nullptr
};

const ObjectSlots sock_objslot = {
        sock_methods,
        nullptr,
        sock_bases,
        nullptr,
        nullptr,
        -1
};

TypeInfo SocketType = {
        AROBJ_HEAD_INIT_TYPE,
        "Socket",
        nullptr,
        nullptr,
        sizeof(Socket),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &sock_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::io::socket::type_socket_ = &SocketType;

bool argon::vm::io::socket::AddrToSockAddr(ArObject *addr, sockaddr_storage *store, socklen_t *len, int family) {
    char *saddr;

    *len = sizeof(sockaddr_in);
    store->ss_family = (short) family;

    switch (family) {
        case AF_INET: {
            auto *in = (sockaddr_in *) store;

            if (!TupleUnpack((Tuple *) addr, "sh", &saddr, &in->sin_port))
                return false;

            in->sin_port = htons(in->sin_port);

            if (inet_pton(family, saddr, &(in->sin_addr.s_addr)) > 0)
                return true;

            break;
        }
        case AF_INET6: {
            auto *in = (sockaddr_in6 *) store;

            if (!TupleUnpack((Tuple *) addr, "shII", &saddr, &in->sin6_port, &in->sin6_flowinfo, &in->sin6_scope_id))
                return false;

            in->sin6_port = htons(in->sin6_port);
            in->sin6_flowinfo = htonl(in->sin6_flowinfo);
            in->sin6_scope_id = htonl(in->sin6_scope_id);
            *len = sizeof(sockaddr_in6);

            if (inet_pton(family, saddr, &((sockaddr_in *) store)->sin_addr.s_addr) >= 0)
                return true;

            break;
        }
            // TODO: AF_UNIX
        default:
            ErrorFormat(kOSError[0], "unsupported address family");
            return false;
    }

    ErrorFormat(kValueError[0], "invalid network address");
    return false;
}

void argon::vm::io::socket::ErrorFromSocket() {
#ifdef _ARGON_PLATFORM_WINDOWS
    auto *error = ErrorNewFromSocket();

    if (error != nullptr)
        argon::vm::Panic((ArObject *) error);

    Release(error);
#else
    ErrorFromErrno(errno);
#endif
}
