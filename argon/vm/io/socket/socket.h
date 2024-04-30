// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_SOCKET_SOCKET_H_
#define ARGON_VM_IO_SOCKET_SOCKET_H_

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif

#include <argon/vm/loop2/evloop.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/error.h>

namespace argon::vm::io::socket {
    constexpr const char *kGAIError[] = {
            (const char *) "GAIError",
    };

    constexpr const int kRecvAllStartSize = 1024;
    constexpr const int kRecvAllIncSize = 1024;

#ifdef _ARGON_PLATFORM_WINDOWS
    constexpr const char *kWSAError[] = {
            (const char *) "WSAError",
    };
#endif

#ifdef _ARGON_PLATFORM_WINDOWS
#define SOCK_HANDLE_INVALID (SockHandle)(~0)
    using SockHandle = unsigned long long;
#else
#define SOCK_HANDLE_INVALID (SockHandle)(-1)
    using SockHandle = int;
#endif

    struct Socket {
        AROBJ_HEAD;

        SockHandle sock;

        int family;
        int type;
        int protocol;

        int timeout;

#ifdef _ARGON_PLATFORM_WINDOWS
        sockaddr_storage addr;
        socklen_t addrlen;

        LPFN_ACCEPTEX AcceptEx;
        LPFN_CONNECTEX ConnectEx;
#else
        loop2::EvLoopQueue *queue;
#endif
    };

    _ARGONAPI const extern datatype::TypeInfo *type_socket_;

    datatype::ArObject *PeerName(const Socket *sock);

    datatype::ArObject *SockAddrToAddr(sockaddr_storage *storage, int family);

    datatype::ArObject *SockName(const Socket *sock);

    bool Accept(Socket *sock);

    bool AddrToSockAddr(datatype::ArObject *addr, sockaddr_storage *store, socklen_t *len, int family);

    bool Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen);

    bool Connect(Socket *sock, const struct sockaddr *addr, socklen_t addrlen);

    bool Close(Socket *sock);

    bool IsInheritable(const Socket *sock);

    bool Listen(const Socket *sock, int backlog);

    bool Recv(Socket *sock, size_t len, int flags, int timeout);

    bool RecvAll(Socket *sock, int flags);

    bool RecvCB(Socket *sock, datatype::ArObject * user_data, loop2::UserCB callback,
                unsigned char *buffer, size_t len, int flags);

    bool RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags, int timeout);

    bool RecvFrom(Socket *sock, size_t len, int flags, int timeout);

    bool Send(Socket *sock, datatype::ArObject *buffer, long size, int flags, int timeout);

    bool Send(Socket *sock, unsigned char *buffer, long size, int flags);

    bool SendCB(Socket * sock, datatype::ArObject * user_data, loop2::UserCB callback,
                unsigned char *buffer, size_t len, int flags);

    bool SendRecvCB(Socket *sock, datatype::ArObject *user_data, loop2::UserCB recv_cb,
                    unsigned char *buffer, size_t len, size_t capacity);

    bool SendTo(Socket *sock, datatype::ArObject *dest, datatype::ArObject *buffer, long size, int flags, int timeout);

    bool SetInheritable(const Socket *sock, bool inheritable);

    datatype::Error *ErrorNewFromSocket();

    int SocketAddrLen(const Socket *sock);

    Socket *Dup(const Socket *sock);

    Socket *SocketNew(int domain, int type, int protocol);

    Socket *SocketNew(int domain, int type, int protocol, SockHandle handle);

    SockHandle Detach(Socket *sock);

    void ErrorFromSocket();

} // namespace argon::vm::io::socket

#endif // !ARGON_VM_IO_SOCKET_SOCKET_H_
