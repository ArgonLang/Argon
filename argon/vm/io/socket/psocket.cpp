// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS

#include <fcntl.h>
#include <unistd.h>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/integer.h>

#include <argon/vm/io/socket/socket.h>

using namespace argon::vm::loop2;
using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

CallbackStatus AcceptCallback(Event *event) {
    sockaddr_storage addr{};
    socklen_t addrlen;
    int remote;

    auto *sock = ((const Socket *) event->initiator);

    remote = accept(sock->sock, (sockaddr *) &addr, &addrlen);
    if (remote < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    auto *ret = SocketNew(sock->family, sock->type, sock->protocol, remote);
    if (ret == nullptr)
        return CallbackStatus::FAILURE;

    argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) ret);

    Release(ret);

    return CallbackStatus::SUCCESS;
}

CallbackStatus ConnectCallback(Event *event) {
    auto *sock = (Socket *) event->initiator;
    socklen_t len = sizeof(int);
    int error;

    getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, &error, &len);

    if (error == 0) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) sock);

        return CallbackStatus::SUCCESS;
    }

    ErrorFromErrno(error);

    return CallbackStatus::FAILURE;
}

CallbackStatus RecvCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = recv(sock->sock,
                      event->buffer.data + event->buffer.length,
                      event->buffer.allocated - event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length = bytes;

    auto *buffer = BytesNewHoldBuffer(event->buffer.data, event->buffer.allocated, event->buffer.length, true);
    if (buffer == nullptr) {
        argon::vm::memory::Free(event->buffer.data);

        return CallbackStatus::FAILURE;
    }

    argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

    Release(buffer);

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvAllCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;
    auto delta = event->buffer.allocated - event->buffer.length;

    auto bytes = recv(sock->sock,
                      event->buffer.data + event->buffer.length,
                      delta, event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length += bytes;

    if (bytes < delta) {
        auto *buffer = BytesNewHoldBuffer(event->buffer.data, event->buffer.allocated, event->buffer.length, true);
        if (buffer == nullptr) {
            argon::vm::memory::Free(event->buffer.data);

            return CallbackStatus::FAILURE;
        }

        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return CallbackStatus::SUCCESS;
    }

    auto *tmp = (unsigned char *) argon::vm::memory::Realloc(event->buffer.data,
                                                             event->buffer.allocated + kRecvAllIncSize);
    if (tmp == nullptr) {
        argon::vm::memory::Free(event->buffer.data);

        return CallbackStatus::FAILURE;
    }

    event->buffer.data = tmp;

    event->buffer.allocated += kRecvAllIncSize;

    return CallbackStatus::RETRY;
}

CallbackStatus RecvFromCallback(Event *event) {
    sockaddr_storage storage{};
    socklen_t addrlen = sizeof(sockaddr_storage);

    auto *sock = (const Socket *) event->initiator;


    auto bytes = recvfrom(sock->sock,
                          event->buffer.data + event->buffer.length,
                          event->buffer.allocated - event->buffer.length,
                          event->flags, (sockaddr *) &storage, &addrlen);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length = bytes;

    auto *remote_addr = SockAddrToAddr(&storage, sock->family);
    if (remote_addr == nullptr) {
        argon::vm::memory::Free(event->buffer.data);

        return CallbackStatus::FAILURE;
    }

    auto *data = BytesNewHoldBuffer(event->buffer.data, event->buffer.allocated, event->buffer.length, true);
    if (data == nullptr) {
        argon::vm::memory::Free(event->buffer.data);

        Release(remote_addr);

        return CallbackStatus::FAILURE;
    }

    auto *ret = TupleNew("oo", data, remote_addr);

    Release(remote_addr);
    Release(data);

    if (ret != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) ret);

        Release(ret);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus RecvIntoCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = recv(sock->sock,
                      event->buffer.data + event->buffer.length,
                      event->buffer.allocated - event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length = bytes;

    auto *buffer = IntNew((IntegerUnderlying) event->buffer.length);

    BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus RecvRawCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = recv(sock->sock,
                      event->buffer.data,
                      event->buffer.allocated,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            ErrorFromSocket();

            event->user_callback(event, event->aux, (int) bytes);

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length = bytes;

    return event->user_callback(event, event->aux, 0);
}

CallbackStatus SendCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = send(sock->sock,
                      event->buffer.data,
                      event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            if (event->buffer.arbuf.buffer != nullptr)
                BufferRelease(&event->buffer.arbuf);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    auto *buffer = IntNew(bytes);

    if (event->buffer.arbuf.buffer != nullptr)
        BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus SendRawCallback(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = send(sock->sock,
                      event->buffer.data,
                      event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            ErrorFromSocket();

            event->user_callback(event, event->aux, (int) bytes);

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    event->buffer.length = bytes;

    return event->user_callback(event, event->aux, 0);
}

CallbackStatus SendRecvCallback(Event *event) {
    auto *sock = (Socket *) event->initiator;

    auto bytes = send(sock->sock,
                      event->buffer.data,
                      event->buffer.length,
                      event->flags);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    if (!RecvCB(sock,
                event->aux,
                event->user_callback,
                event->buffer.data,
                event->buffer.allocated,
                0))
        return CallbackStatus::FAILURE;

    return CallbackStatus::CONTINUE;
}

CallbackStatus SendToCallback(Event *event) {
    sockaddr_storage storage{};
    socklen_t addrlen;

    auto *sock = (const Socket *) event->initiator;

    if (!AddrToSockAddr(event->aux, &storage, &addrlen, sock->family))
        return CallbackStatus::FAILURE;

    auto bytes = sendto(sock->sock,
                        event->buffer.arbuf.buffer,
                        event->buffer.length,
                        event->flags, (sockaddr *) &storage, addrlen);

    if (bytes < 0) {
        if (errno != EAGAIN) {
            BufferRelease(&event->buffer.arbuf);

            ErrorFromSocket();

            return CallbackStatus::FAILURE;
        }

        return CallbackStatus::RETRY;
    }

    auto *buffer = IntNew(bytes);

    BufferRelease(&event->buffer.arbuf);

    if (buffer != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) buffer);

        Release(buffer);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

bool argon::vm::io::socket::Accept(Socket *sock) {
    Event *event;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    event->callback = AcceptCallback;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN)) {
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
    auto *loop = EvLoopGet();

    auto *event = EventNew(loop, (ArObject *) sock);
    if (event == nullptr)
        return false;

    if (connect(sock->sock, addr, len) < 0) {
        if (errno != EINPROGRESS) {
            EventDel(event);

            ErrorFromSocket();

            return false;
        }

        event->callback = ConnectCallback;

        if (!AddEvent(loop, sock->queue, event, EvLoopQueueDirection::OUT)) {
            EventDel(event);
            return false;
        }
    } else
        EventDel(event);

    return true;
}

bool argon::vm::io::socket::Close(Socket *sock) {
    int times = 3;
    int err;

    do {
        err = close(sock->sock);
        times--;
    } while (err != 0 && errno == EINTR && times > 0);

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

bool argon::vm::io::socket::Recv(Socket *sock, size_t len, int flags, int timeout) {
    Event *event;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(len)) == nullptr) {
        EventDel(event);
        return false;
    }

    event->buffer.length = 0;
    event->buffer.allocated = len;

    event->callback = RecvCallback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN, timeout)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvAll(Socket *sock, int flags) {
    Event *event;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(kRecvAllStartSize)) == nullptr) {
        EventDel(event);
        return false;
    }

    event->buffer.length = 0;
    event->buffer.allocated = kRecvAllStartSize;

    event->callback = RecvAllCallback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN, sock->timeout)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvCB(Socket *sock, ArObject *user_data, UserCB callback,
                                   unsigned char *buffer, size_t len, int flags) {
    Event *event;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    event->buffer.data = buffer;
    event->buffer.length = 0;
    event->buffer.allocated = len;

    event->aux = IncRef(user_data);

    event->callback = RecvRawCallback;
    event->user_callback = callback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN, sock->timeout)) {
        EventDel(event);

        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags, int timeout) {
    Event *event;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::WRITE)) {
        EventDel(event);
        return false;
    }

    event->buffer.data = event->buffer.arbuf.buffer + offset;
    event->buffer.length = 0;
    event->buffer.allocated = event->buffer.arbuf.length - offset;

    event->callback = RecvIntoCallback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN, timeout)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvFrom(Socket *sock, size_t len, int flags, int timeout) {
    Event *event;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->buffer.data = (unsigned char *) memory::Alloc(len)) == nullptr) {
        EventDel(event);
        return false;
    }

    event->buffer.length = 0;
    event->buffer.allocated = len;

    event->callback = RecvFromCallback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::IN, timeout)) {
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, long size, int flags, int timeout) {
    Event *event;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::READ)) {
        EventDel(event);
        return false;
    }

    event->buffer.data = event->buffer.arbuf.buffer;
    event->buffer.length = size;

    if (size < 0 || size > event->buffer.arbuf.length)
        event->buffer.length = event->buffer.arbuf.length;

    event->callback = SendCallback;
    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::OUT, timeout)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Send(Socket *sock, unsigned char *buffer, long size, int flags) {
    Event *event;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    event->buffer.arbuf.buffer = nullptr;

    event->buffer.data = buffer;
    event->buffer.length = size;

    event->callback = SendCallback;
    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::OUT, sock->timeout)) {
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::SendCB(Socket *sock, ArObject *user_data, UserCB callback,
                                   unsigned char *buffer, size_t len, int flags) {
    Event *event;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    event->buffer.data = buffer;
    event->buffer.length = len;

    event->aux = IncRef(user_data);

    event->callback = SendRawCallback;
    event->user_callback = callback;

    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::OUT, sock->timeout)) {
        EventDel(event);

        return false;
    }

    return true;
}

bool argon::vm::io::socket::SendRecvCB(Socket *sock, ArObject *user_data, UserCB rcb,
                                       unsigned char *buffer, size_t len, size_t capacity) {
    Event *event;

    if (len == 0)
        return RecvCB(sock, user_data, rcb, buffer, capacity, 0);

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    event->buffer.data = buffer;
    event->buffer.length = len;
    event->buffer.allocated = capacity;

    event->aux = IncRef(user_data);

    event->callback = SendRecvCallback;
    event->user_callback = rcb;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::OUT, sock->timeout)) {
        EventDel(event);

        return false;
    }

    return true;
}

bool argon::vm::io::socket::SendTo(Socket *sock, datatype::ArObject *dest, datatype::ArObject *buffer,
                                   long size, int flags, int timeout) {
    Event *event;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((event = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::READ)) {
        EventDel(event);
        return false;
    }

    event->buffer.length = size;

    if (size < 0 || size > event->buffer.arbuf.length)
        event->buffer.length = event->buffer.arbuf.length;

    event->aux = IncRef(dest);
    event->callback = SendToCallback;
    event->flags = flags;

    if (!AddEvent(EvLoopGet(), sock->queue, event, EvLoopQueueDirection::OUT, timeout)) {
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
    sock->timeout = 0;

    if ((sock->queue = QueueNew(sock->sock)) == nullptr) {
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