// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_SOCKET_SOCKET_H_
#define ARGON_VM_IO_SOCKET_SOCKET_H_

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#else

#include <sys/socket.h>
#endif

#include <vm/datatype/arobject.h>
#include <vm/datatype/bytes.h>
#include <vm/datatype/error.h>

namespace argon::vm::io::socket {
    constexpr const char *kGAIError[] = {
            (const char *) "GAIError",
    };

#ifdef _ARGON_PLATFORM_WINDOWS
    constexpr const char *kWSAError[] = {
            (const char *) "WSAError",
    };
#endif

#ifdef _ARGON_PLATFORM_WINDOWS
#define SOCK_HANDLE_INVALID (sock_handle)(~0)
    using SockHandle = unsigned long long;
#else
#define SOCK_HANDLE_INVALID (sock_handle)(-1)
    using SockHandle = int;
#endif

    struct Socket {
        AROBJ_HEAD;

        SockHandle sock;

        int family;
        int type;
        int protocol;

        sockaddr_storage addr;

#ifdef _ARGON_PLATFORM_WINDOWS
        LPFN_ACCEPTEX AcceptEx;
        LPFN_CONNECTEX ConnectEx;
#endif
    };

    const extern datatype::TypeInfo *type_socket_;

    datatype::ArSSize Connect(Socket *sock, const struct sockaddr *addr, socklen_t addrlen);

    datatype::ArSSize RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags);

    datatype::ArSSize Send(Socket *sock, datatype::ArObject *buffer, int flags);

    bool Accept(Socket *sock, Socket **out);

    bool AddrToSockAddr(datatype::ArObject *addr, sockaddr_storage *store, socklen_t *len, int family);

    bool Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen);

    bool Listen(const Socket *sock, int backlog);

    bool Recv(Socket *sock, datatype::Bytes **out, size_t len, int flags);

    datatype::Error *ErrorNewFromSocket();

    Socket *SocketNew(int domain, int type, int protocol);

    void ErrorFromSocket();

} // namespace argon::vm::io::socket

#endif // !ARGON_VM_IO_SOCKET_SOCKET_H_
