// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_SOCKET_SOCKET_H_
#define ARGON_MODULE_SOCKET_SOCKET_H_

#include <object/datatype/tuple.h>

#include <object/arobject.h>

namespace argon::module::socket {
#ifdef _ARGON_PLATFORM_WINDOWS
#define SOCK_HANDLE_INVALID (sock_handle)(~0)
    using sock_handle = unsigned int;
#else
#define SOCK_HANDLE_INVALID (sock_handle)(-1)
    using sock_handle = int;
#endif

    struct Socket : argon::object::ArObject {
        sock_handle sock;
        int family;

        bool blocking;
    };

    argon::object::ArObject *ErrorNewFromSocket();

    argon::object::ArObject *ErrorSetFromSocket();

    extern const argon::object::TypeInfo *type_socket_;

    bool ArAddrToSockAddr(argon::object::ArObject *tuple, struct sockaddr_storage *addrstore, int *socklen, int family);

    int SocketGetAddrLen(Socket *socket);

    int SocketGetFlags(Socket *socket, int type);

    inline bool SocketIsNonBlock(Socket *socket) { return !socket->blocking; }

    bool SocketSetFlags(Socket *socket, int type, long flags);

    bool SocketSetNonBlock(Socket *socket, bool nonblock);

    bool SocketSetInheritable(Socket *socket, bool inheritable);

    int CloseHandle(sock_handle handle);

    inline int Close(Socket *socket) {
        int err;

        if ((err = CloseHandle(socket->sock)) == 0)
            socket->sock = SOCK_HANDLE_INVALID;

        return err;
    }

    Socket *SocketNew(int domain, int type, int protocol);

    Socket *SocketNew(sock_handle handle, int family);

    argon::object::ArObject *SockAddrToArAddr(struct sockaddr_storage *storage, int family);
}

#endif // !ARGON_MODULE_SOCKET_SOCKET_H_
