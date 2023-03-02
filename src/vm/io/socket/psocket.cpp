// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS

#include <fcntl.h>
#include <unistd.h>

#include <vm/runtime.h>

#include <vm/loop/evloop.h>

#include "socket.h"
#include "vm/datatype/integer.h"

using namespace argon::vm::loop;
using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

bool AcceptCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;
    auto *remote = (Socket *) event->aux;

    remote->sock = accept(sock->sock, (sockaddr *) &remote->addr, &remote->addrlen);
    if (remote->sock < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Release(event->aux);

            ErrorFromSocket();
        }

        return false;
    }

    argon::vm::ReplaceFrameTopValue(event->aux);

    return true;
}

bool ConnectCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto res = connect(sock->sock, (sockaddr *) event->buffer.data, (socklen_t) event->buffer.length);

    if (res < 0) {
        if (errno != EINPROGRESS) {
            argon::vm::memory::Free(event->buffer.data);

            ErrorFromSocket();
        }

        return false;
    } else
        argon::vm::memory::Free(event->buffer.data);

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

    argon::vm::ReplaceFrameTopValue((ArObject *) buffer);

    Release(buffer);

    return true;
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
        argon::vm::ReplaceFrameTopValue((ArObject *) buffer);

        Release(buffer);

        return true;
    }

    return false;
}

bool SendCallBack(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    auto bytes = send(sock->sock,
                      event->buffer.arbuf.buffer,
                      event->buffer.arbuf.length,
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
        argon::vm::ReplaceFrameTopValue((ArObject *) buffer);

        Release(buffer);

        return true;
    }

    return false;
}

bool argon::vm::io::socket::Accept(Socket *sock) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if ((event->aux = (ArObject *) SocketNew(sock->family, sock->type, sock->protocol)) == nullptr) {
        EventDel(event);
        return false;
    }

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
        memory::Free(event->buffer.data);
        EventDel(event);
        return false;
    }

    return true;
}

bool argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, int flags) {
    Event *event;

    if ((event = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &event->buffer.arbuf, BufferFlags::READ)) {
        EventDel(event);
        return false;
    }

    event->callback = SendCallBack;
    event->flags = flags;

    if (!EventLoopAddEvent(GetEventLoop(), sock->queue, event, EventDirection::OUT)) {
        BufferRelease(&event->buffer.arbuf);
        EventDel(event);
        return false;
    }

    return true;
}

Error *argon::vm::io::socket::ErrorNewFromSocket() {
    return ErrorNewFromErrno(errno);
}

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol) {
    Socket *sock;
    SockHandle handle;

    if ((handle = ::socket(domain, type, protocol)) < 0) {
        ErrorFromSocket();
        return nullptr;
    }

    auto flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        ErrorFromErrno(errno);

        close(handle);

        return nullptr;
    }

    flags |= O_NONBLOCK;

    if (fcntl(handle, F_SETFL, flags) < 0) {
        ErrorFromErrno(errno);

        close(handle);

        return nullptr;
    }

    if ((sock = MakeObject<Socket>(type_socket_)) == nullptr) {
        close(handle);

        return nullptr;
    }

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

#endif