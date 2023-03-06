// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS

#include <fcntl.h>
#include <unistd.h>

#include <vm/runtime.h>

#include <vm/loop/evloop.h>

#include <vm/datatype/boolean.h>
#include <vm/datatype/integer.h>

#include "socket.h"


using namespace argon::vm::loop;
using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

bool AcceptCallBack(Event *event) {
    sockaddr_storage addr{};
    socklen_t addrlen;
    int remote;

    auto *sock = ((const Socket *) event->initiator);

    remote = accept(sock->sock, (sockaddr *) &addr, &addrlen);
    if (remote < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            ErrorFromSocket();

        return false;
    }

    auto *ret = SocketNew(sock->family, sock->type, sock->protocol, remote);
    if (ret == nullptr)
        return false;

    argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) ret);

    Release(ret);

    return true;
}

bool ConnectResultCallBack(Event *event) {
    auto *sock = (Socket *) event->initiator;
    socklen_t len = sizeof(int);
    int error;

    getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, &error, &len);

    argon::vm::memory::Free(event->buffer.data);

    if (error == 0) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) sock);

        return true;
    }

    ErrorFromErrno(error);

    return false;
}

bool ConnectCallBack(Event *event) {
    event->callback = ConnectResultCallBack;

    if (connect(((const Socket *) event->initiator)->sock, (sockaddr *) event->buffer.data,
                (socklen_t) event->buffer.length) < 0) {
        if (errno != EINPROGRESS) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();
        }

        return false;
    }

    return true;
}

bool RecvCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = recv(sock->sock,
                      event->buffer.data + event->buffer.length,
                      event->buffer.allocated - event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();
        }

        return false;
    }

    event->buffer.length += bytes;
    if (bytes > 0 && event->buffer.length < event->buffer.allocated)
        return false;

    auto *buffer = BytesNewHoldBuffer(event->buffer.data, event->buffer.allocated, event->buffer.length, true);
    if (buffer == nullptr) {
        argon::vm::memory::Free(event->buffer.data);

        return false;
    }

    argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

    Release(buffer);

    return true;
}

bool RecvFromCallBack(Event *event) {
    sockaddr_storage storage{};
    socklen_t addrlen = sizeof(sockaddr_storage);

    auto *sock = (const Socket *) event->initiator;


    auto bytes = recvfrom(sock->sock,
                          event->buffer.data + event->buffer.length,
                          event->buffer.allocated - event->buffer.length,
                          event->flags, (sockaddr *) &storage, &addrlen);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);

            ErrorFromSocket();
        }

        return false;
    }

    event->buffer.length += bytes;
    if (sock->type != SOCK_DGRAM && bytes > 0 && event->buffer.length < event->buffer.allocated)
        return false;

    auto *remote_addr = SockAddrToAddr(&storage, sock->family);
    if (remote_addr == nullptr) {
        BufferRelease(&event->buffer.arbuf);
        return false;
    }

    auto *data = BytesNewHoldBuffer(event->buffer.data, event->buffer.allocated, event->buffer.length, true);
    if (data == nullptr) {
        BufferRelease(&event->buffer.arbuf);
        Release(remote_addr);
        return false;
    }

    auto *ret = TupleNew("oo", data, remote_addr);

    Release(remote_addr);
    Release(data);

    if (ret != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) ret);

        Release(ret);

        return true;
    }

    return false;
}

bool RecvIntoCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = recv(sock->sock,
                      event->buffer.data + event->buffer.length,
                      event->buffer.allocated - event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);

            ErrorFromSocket();
        }

        return false;
    }

    event->buffer.length += bytes;
    if (bytes > 0 && event->buffer.length < event->buffer.allocated)
        return false;

    auto *buffer = IntNew((IntegerUnderlying) event->buffer.length);

    BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return true;
    }

    return false;
}

bool SendCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = send(sock->sock,
                      event->buffer.arbuf.buffer,
                      event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);
            ErrorFromSocket();
        }

        return false;
    }

    auto *buffer = IntNew(bytes);

    BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return true;
    }

    return false;
}

bool SendToCallBack(Event *event) {
    sockaddr_storage storage{};
    socklen_t addrlen;

    auto *sock = (const Socket *) event->initiator;

    if (!AddrToSockAddr(event->aux, &storage, &addrlen, sock->family))
        return false;

    auto bytes = sendto(sock->sock,
                        event->buffer.arbuf.buffer,
                        event->buffer.length,
                        event->flags, (sockaddr *) &storage, addrlen);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);
            ErrorFromSocket();
        }

        return false;
    }

    auto *buffer = IntNew(bytes);

    BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return true;
    }

    return false;
}

bool argon::vm::io::socket::Accept(Socket *sock) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    event->callback = AcceptCallBack;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::IN)) {
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen) {
    if (::bind(sock->sock, addr, addrlen) != 0) {
        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::Connect(Socket *sock, const sockaddr *addr, socklen_t len) {
    auto *loop = GetEventLoop();

    auto *event = EventNew(loop, (ArObject *) sock);
    if (event == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(len)) == nullptr) {
        EventDel(event);
        return false;
    }

    memory::MemoryCopy(event->buffer.data, addr, len);
    event->buffer.length = len;

    event->callback = ConnectCallBack;

    if (!EventLoopAddEvent(loop, sock->queue, event, EventDirection::OUT)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Close(Socket *sock) {
    int times = 3;
    int err;

    do {
        err = close(sock->sock);
    } while (err != 0 && errno == EINTR && times-- > 0);

    if (err == 0) {
        sock->sock = SOCK_HANDLE_INVALID;
        return true;
    }

    return false;
}

bool argon::vm::io::socket::IsInheritable(const Socket *sock) {
    int flags = fcntl(sock->sock, F_GETFD, 0);
    return (flags & FD_CLOEXEC) != FD_CLOEXEC;
}

bool argon::vm::io::socket::Listen(const Socket *sock, int backlog) {
    return listen(sock->sock, backlog) == 0;
}

bool argon::vm::io::socket::Recv(Socket *sock, size_t len, int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(len)) == nullptr) {
        EventDel(event);
        return false;
    }

    event->buffer.length = 0;
    event->buffer.allocated = len;

    event->callback = RecvCallBack;

    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::IN)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::WRITE)) {
        EventDel(event);
        return false;
    }

    event->buffer.data = event->buffer.arbuf.buffer + offset;
    event->buffer.length = 0;
    event->buffer.allocated = event->buffer.arbuf.length - offset;

    event->callback = RecvIntoCallBack;

    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::IN)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvFrom(Socket *sock, size_t len, int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(len)) == nullptr) {
        EventDel(event);
        return false;
    }

    event->buffer.length = 0;
    event->buffer.allocated = len;

    event->callback = RecvFromCallBack;

    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::IN)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, long size, int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::READ)) {
        EventDel(event);
        return false;
    }

    event->buffer.length = size;

    if (size < 0 || size > event->buffer.arbuf.length)
        event->buffer.length = event->buffer.arbuf.length;

    event->callback = SendCallBack;
    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::OUT)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::SendTo(Socket *sock, datatype::ArObject *dest, datatype::ArObject *buffer, long size,
                                   int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::READ)) {
        EventDel(event);
        return false;
    }

    event->buffer.length = size;

    if (size < 0 || size > event->buffer.arbuf.length)
        event->buffer.length = event->buffer.arbuf.length;

    event->aux = IncRef(dest);
    event->callback = SendToCallBack;
    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::OUT)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::SetInheritable(const Socket *sock, bool inheritable) {
    int flags = fcntl(sock->sock, F_GETFD, 0);

    flags = !inheritable ? flags | FD_CLOEXEC : flags & (~FD_CLOEXEC);

    if (fcntl(sock->sock, F_SETFD, flags) < 0) {
        ErrorFromErrno(errno);

        return false;
    }

    return true;
}

Error *argon::vm::io::socket::ErrorNewFromSocket() {
    return ErrorNewFromErrno(errno);
}

Socket *argon::vm::io::socket::Dup(const Socket *sock) {
    Socket *ret;
    SockHandle handle;

    if ((handle = dup(sock->sock)) < 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    if ((ret = SocketNew(sock->family, sock->type, sock->protocol, handle)) == nullptr)
        close(handle);

    return ret;
}

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol) {
    Socket *sock;
    SockHandle handle;

    if ((handle = ::socket(domain, type, protocol)) < 0) {
        ErrorFromSocket();
        return nullptr;
    }

    if ((sock = SocketNew(domain, type, protocol, handle)) == nullptr) {
        close(handle);
        return nullptr;
    }

    return sock;
}

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol, SockHandle handle) {
    Socket *sock;

    auto flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        ErrorFromErrno(errno);

        return nullptr;
    }

    flags |= O_NONBLOCK;

    if (fcntl(handle, F_SETFL, flags) < 0) {
        ErrorFromErrno(errno);

        return nullptr;
    }

    if ((sock = MakeObject<Socket>(type_socket_)) == nullptr)
        return nullptr;

    sock->sock = handle;

    sock->family = domain;
    sock->type = type;
    sock->protocol = protocol;

    if ((sock->queue = loop::EventQueueNew(sock->sock)) == nullptr) {
        Release(sock);
        return nullptr;
    }

    return sock;
}

SockHandle argon::vm::io::socket::Detach(Socket *sock) {
    auto handle = sock->sock;

    sock->sock = SOCK_HANDLE_INVALID;

    return handle;
}

ArObject *argon::vm::io::socket::PeerName(const Socket *sock) {
    sockaddr_storage storage{};
    int namelen;

    if ((namelen = SocketAddrLen(sock)) == 0)
        return nullptr;

    if (getpeername(sock->sock, (sockaddr *) &storage, (socklen_t *) &namelen) != 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    return SockAddrToAddr(&storage, sock->family);
}

ArObject *argon::vm::io::socket::SockName(const Socket *sock) {
    sockaddr_storage storage{};
    int namelen;

    if ((namelen = SocketAddrLen(sock)) == 0)
        return nullptr;

    if (getsockname(sock->sock, (sockaddr *) &storage, (socklen_t *) &namelen) != 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    return SockAddrToAddr(&storage, sock->family);
}

#endif