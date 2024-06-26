// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/integer.h>

#include <argon/vm/io/socket/socket.h>

#include <argon/vm/loop2/evloop.h>

using namespace argon::vm::loop2;
using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

// Prototypes

bool LoadWSAExtension(SOCKET socket, GUID guid, void **target);

CallbackStatus RecvAllStarter(Event *event);

// EOL

CallbackStatus AcceptCallBack(Event *event) {
    argon::vm::FiberSetAsyncResult(event->fiber, event->aux);

    return CallbackStatus::SUCCESS;
}

CallbackStatus AcceptStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;
    auto *remote = (Socket *) event->aux;

    event->callback = AcceptCallBack;

    auto result = sock->AcceptEx(
            sock->sock,
            remote->sock,
            &remote->addr,
            0,
            0,
            sizeof(sockaddr_storage),
            nullptr,
            (OVERLAPPED *) event);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus ConnectCallBack(Event *event) {
    argon::vm::memory::Free(event->buffer.data);

    argon::vm::FiberSetAsyncResult(event->fiber, event->initiator);

    return CallbackStatus::SUCCESS;
}

CallbackStatus ConnectStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = ConnectCallBack;

    auto result = sock->ConnectEx(sock->sock,
                                  (sockaddr *) event->buffer.data,
                                  (socklen_t) event->buffer.length,
                                  nullptr,
                                  0,
                                  nullptr,
                                  (OVERLAPPED *) event);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        argon::vm::memory::Free(event->buffer.data);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvCallBack(Event *event) {
    auto *bytes = BytesNewHoldBuffer(
            (unsigned char *) event->buffer.wsa.buf,
            event->buffer.allocated,
            event->buffer.wsa.len,
            true);

    if (bytes == nullptr) {
        argon::vm::memory::Free(event->buffer.wsa.buf);
        return CallbackStatus::FAILURE;
    }

    argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) bytes);

    Release(bytes);

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RecvCallBack;

    auto result = WSARecv(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        argon::vm::memory::Free(event->buffer.wsa.buf);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvAllCallBack(Event *event) {
    auto delta = event->buffer.allocated - event->buffer.length;

    if (event->buffer.wsa.len < delta) {
        auto *bytes = BytesNewHoldBuffer(
                (unsigned char *) event->buffer.data,
                event->buffer.allocated,
                event->buffer.length + event->buffer.wsa.len,
                true);

        if (bytes == nullptr) {
            argon::vm::memory::Free(event->buffer.data);

            return CallbackStatus::FAILURE;
        }

        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) bytes);

        Release(bytes);

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

    event->buffer.length += event->buffer.wsa.len;

    event->buffer.wsa.buf = (char *) (tmp + event->buffer.length);
    event->buffer.wsa.len = event->buffer.allocated - event->buffer.length;

    if (RecvAllStarter(event) != CallbackStatus::SUCCESS) {
        argon::vm::memory::Free(event->buffer.data);

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::RETRY;
}

CallbackStatus RecvAllStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RecvAllCallBack;

    auto result = WSARecv(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        argon::vm::memory::Free(event->buffer.data);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus RawCallBack(Event *event) {
    event->buffer.data = (unsigned char *) event->buffer.wsa.buf;
    event->buffer.length = event->buffer.wsa.len;

    return event->user_callback(event, event->aux, 0);
}

CallbackStatus RecvRawStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RawCallBack;

    auto result = WSARecv(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvFromCallBack(Event *event) {
    auto *remote_addr = SockAddrToAddr((sockaddr_storage *) event->buffer.data, ((Socket *) event->initiator)->family);
    if (remote_addr == nullptr) {
        argon::vm::memory::Free(event->buffer.wsa.buf);
        argon::vm::memory::Free(event->buffer.data);

        return CallbackStatus::FAILURE;
    }

    argon::vm::memory::Free(event->buffer.data);

    auto *data = BytesNewHoldBuffer((unsigned char *) event->buffer.wsa.buf, event->buffer.allocated,
                                    event->buffer.wsa.len, true);
    if (data == nullptr) {
        argon::vm::memory::Free(event->buffer.wsa.buf);

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

CallbackStatus RecvFromStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RecvFromCallBack;

    auto result = WSARecvFrom(sock->sock,
                              &event->buffer.wsa,
                              1,
                              nullptr,
                              (DWORD *) &event->flags,
                              (sockaddr *) event->buffer.data,
                              (LPINT) &event->buffer.length,
                              event,
                              nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        argon::vm::memory::Free(event->buffer.wsa.buf);
        argon::vm::memory::Free(event->buffer.data);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus RecvIntoCallBack(Event *event) {
    BufferRelease(&event->buffer.arbuf);

    auto *bytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
    if (bytes != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) bytes);

        Release(bytes);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus RecvIntoStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RecvIntoCallBack;

    auto result = WSARecv(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&event->buffer.arbuf);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus SendCallBack(Event *event) {
    BufferRelease(&event->buffer.arbuf);

    auto *wbytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
    if (wbytes != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) wbytes);

        Release(wbytes);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus SendRecvCallBack(Event *event) {
    if (!RecvCB((Socket *) event->initiator,
                event->aux,
                event->user_callback,
                (unsigned char *) event->buffer.wsa.buf,
                event->buffer.allocated,
                0))
        return CallbackStatus::FAILURE;

    return CallbackStatus::CONTINUE;
}

CallbackStatus SendCBStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = RawCallBack;

    auto result = WSASend(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD) event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus SendStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = SendCallBack;

    auto result = WSASend(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD) event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&event->buffer.arbuf);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus SendRecvStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = SendRecvCallBack;

    auto result = WSASend(sock->sock,
                          &event->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD) event->flags,
                          event,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

CallbackStatus SendToCallBack(Event *event) {
    BufferRelease(&event->buffer.arbuf);

    argon::vm::memory::Free(event->buffer.data);

    auto *wbytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
    if (wbytes != nullptr) {
        argon::vm::FiberSetAsyncResult(event->fiber, (ArObject *) wbytes);

        Release(wbytes);

        return CallbackStatus::SUCCESS;
    }

    return CallbackStatus::FAILURE;
}

CallbackStatus SendToStarter(Event *event) {
    auto *sock = (const Socket *) event->initiator;

    event->callback = SendToCallBack;

    auto result = WSASendTo(sock->sock,
                            &event->buffer.wsa,
                            1,
                            nullptr,
                            (DWORD) event->flags,
                            (sockaddr *) event->buffer.data,
                            (int) event->buffer.length,
                            event,
                            nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&event->buffer.arbuf);

        argon::vm::memory::Free(event->buffer.data);

        ErrorFromSocket();

        return CallbackStatus::FAILURE;
    }

    return CallbackStatus::SUCCESS;
}

bool argon::vm::io::socket::Accept(Socket *sock) {
    constexpr GUID wsacceptex = WSAID_ACCEPTEX;
    Socket *remote;

    if (sock->AcceptEx == nullptr && !LoadWSAExtension(sock->sock, wsacceptex, (void **) &sock->AcceptEx))
        return false;

    if ((remote = SocketNew(sock->family, sock->type, sock->protocol)) == nullptr)
        return false;

    auto *ovr = EventNew(EvLoopGet(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    ovr->callback = AcceptStarter;

    ovr->aux = (ArObject *) remote;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen) {
    if (::bind(sock->sock, addr, addrlen) != 0) {
        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::Connect(Socket *sock, const sockaddr *addr, socklen_t len) {
    constexpr GUID wconnectex = WSAID_CONNECTEX;
    sockaddr_in local{};

    if (sock->ConnectEx == nullptr && !LoadWSAExtension(sock->sock, wconnectex, (void **) &sock->ConnectEx))
        return false;

    // Bind socket
    local.sin_family = (short) sock->family;
    local.sin_addr.S_un.S_addr = INADDR_ANY;

    if (!Bind(sock, (const struct sockaddr *) &local, sizeof(sockaddr_in)))
        return false;

    auto *ovr = EventNew(EvLoopGet(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    ovr->buffer.length = len;
    if ((ovr->buffer.data = (unsigned char *) memory::Alloc(sizeof(sockaddr_storage))) == nullptr) {
        EventDel(ovr);
        return false;
    }

    memory::MemoryCopy(ovr->buffer.data, addr, len);

    ovr->callback = ConnectStarter;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::Close(Socket *sock) {
    int times = 3;
    int err;

    do {
        if (closesocket(sock->sock) == 0) {
            sock->sock = SOCK_HANDLE_INVALID;

            return true;
        }

        err = WSAGetLastError();

        times--;
    } while ((err == WSAEINTR || err == WSAEINPROGRESS) && times > 0);

    return false;
}

bool argon::vm::io::socket::IsInheritable(const Socket *sock) {
    unsigned long flags;

    if (GetHandleInformation((HANDLE) sock->sock, &flags) == 0) {
        ErrorFromWinErr();
        return false;
    }

    return (flags & HANDLE_FLAG_INHERIT) == HANDLE_FLAG_INHERIT;
}

bool argon::vm::io::socket::Listen(const Socket *sock, int backlog) {
    return listen(sock->sock, backlog) == 0;
}

bool argon::vm::io::socket::Recv(Socket *sock, size_t len, int flags, int timeout) {
    Event *ovr;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((ovr->buffer.wsa.buf = (char *) memory::Alloc(len)) == nullptr) {
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = (u_long) len;
    ovr->buffer.allocated = len;

    ovr->callback = RecvStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr, timeout);
}

bool argon::vm::io::socket::RecvAll(Socket *sock, int flags) {
    Event *ovr;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((ovr->buffer.wsa.buf = (char *) memory::Alloc(kRecvAllStartSize)) == nullptr) {
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = (u_long) kRecvAllStartSize;

    ovr->buffer.data = (unsigned char *) ovr->buffer.wsa.buf;

    ovr->buffer.allocated = kRecvAllStartSize;
    ovr->buffer.length = 0;

    ovr->callback = RecvAllStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::RecvCB(Socket *sock, ArObject *user_data, loop2::UserCB callback,
                                   unsigned char *buffer, size_t len, int flags) {
    Event *ovr;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    ovr->buffer.wsa.buf = (char *) buffer;
    ovr->buffer.wsa.len = (u_long) len;

    ovr->user_callback = callback;
    ovr->aux = IncRef(user_data);

    ovr->callback = RecvRawStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::RecvFrom(Socket *sock, size_t len, int flags, int timeout) {
    Event *ovr;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if ((ovr->buffer.wsa.buf = (char *) memory::Alloc(len)) == nullptr) {
        EventDel(ovr);

        return false;
    }

    // Remote address goes here!
    ovr->buffer.length = sizeof(sockaddr_storage);
    if ((ovr->buffer.data = (unsigned char *) memory::Alloc(ovr->buffer.length)) == nullptr) {
        argon::vm::memory::Free(ovr->buffer.wsa.buf);
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = (u_long) len;
    ovr->buffer.allocated = len;

    ovr->callback = RecvFromStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr, timeout);
}

bool argon::vm::io::socket::RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags, int timeout) {
    Event *ovr = EventNew(EvLoopGet(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    if (timeout == 0)
        timeout = sock->timeout;

    if (!BufferGet(buffer, &ovr->buffer.arbuf, BufferFlags::WRITE)) {
        EventDel(ovr);

        return false;
    }

    if (offset >= ovr->buffer.arbuf.length) {
        ErrorFormat(kOverflowError[0], kOverflowError[2], AR_TYPE_NAME(buffer), ovr->buffer.arbuf.length, offset);

        BufferRelease(&ovr->buffer.arbuf);

        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.buf = (char *) ovr->buffer.arbuf.buffer + offset;
    ovr->buffer.wsa.len = (u_long) ovr->buffer.arbuf.length - offset;

    ovr->callback = RecvIntoStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr, timeout);
}

bool argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, long size, int flags, int timeout) {
    Event *ovr;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &ovr->buffer.arbuf, BufferFlags::READ)) {
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = size;
    if (size < 0 || size > ovr->buffer.arbuf.length)
        ovr->buffer.wsa.len = (unsigned long) ovr->buffer.arbuf.length;

    ovr->buffer.wsa.buf = (char *) ovr->buffer.arbuf.buffer;

    ovr->callback = SendStarter;
    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr, timeout);
}

bool argon::vm::io::socket::Send(Socket *sock, unsigned char *buffer, long size, int flags) {
    Event *ovr;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    ovr->buffer.wsa.buf = (char *) buffer;
    ovr->buffer.wsa.len = size;

    ovr->callback = SendStarter;
    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::SendCB(Socket *sock, ArObject *user_data, loop2::UserCB callback,
                                   unsigned char *buffer, size_t len, int flags) {
    Event *ovr;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    ovr->buffer.wsa.buf = (char *) buffer;
    ovr->buffer.wsa.len = (unsigned long) len;

    ovr->aux = IncRef(user_data);
    ovr->user_callback = callback;

    ovr->callback = SendCBStarter;
    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::SendRecvCB(Socket *sock, ArObject *user_data, UserCB recv_cb,
                                       unsigned char *buffer, size_t len, size_t capacity) {
    Event *ovr;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    ovr->buffer.wsa.buf = (char *) buffer;
    ovr->buffer.wsa.len = len;
    ovr->buffer.allocated = capacity;

    ovr->aux = IncRef(user_data);
    ovr->user_callback = recv_cb;

    ovr->callback = SendRecvStarter;

    return AddEvent(EvLoopGet(), ovr);
}

bool argon::vm::io::socket::SendTo(Socket *sock, datatype::ArObject *dest, datatype::ArObject *buffer, long size,
                                   int flags, int timeout) {
    Event *ovr;

    if (timeout == 0)
        timeout = sock->timeout;

    if ((ovr = EventNew(EvLoopGet(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &ovr->buffer.arbuf, BufferFlags::READ)) {
        EventDel(ovr);

        return false;
    }

    // Remote address here!
    ovr->buffer.length = sizeof(sockaddr_storage);
    if ((ovr->buffer.data = (unsigned char *) memory::Alloc(ovr->buffer.length)) == nullptr) {
        BufferRelease(&ovr->buffer.arbuf);

        EventDel(ovr);

        return false;
    }

    if (!AddrToSockAddr(dest, (sockaddr_storage *) ovr->buffer.data, (socklen_t *) &ovr->buffer.length, sock->family)) {
        argon::vm::memory::Free(ovr->buffer.data);

        BufferRelease(&ovr->buffer.arbuf);

        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = size;
    if (size < 0 || size > ovr->buffer.arbuf.length)
        ovr->buffer.wsa.len = (unsigned long) ovr->buffer.arbuf.length;

    ovr->buffer.wsa.buf = (char *) ovr->buffer.arbuf.buffer;

    ovr->callback = SendToStarter;

    ovr->flags = flags;

    return AddEvent(EvLoopGet(), ovr, timeout);
}

bool argon::vm::io::socket::SetInheritable(const Socket *sock, bool inheritable) {
    return SetHandleInformation((HANDLE) sock->sock,
                                HANDLE_FLAG_INHERIT,
                                inheritable ? HANDLE_FLAG_INHERIT : 0);
}

bool LoadWSAExtension(SOCKET socket, GUID guid, void **target) {
    DWORD bytes;

    int result = WSAIoctl(socket,
                          SIO_GET_EXTENSION_FUNCTION_POINTER,
                          &guid,
                          sizeof(guid),
                          (void *) target,
                          sizeof(*target),
                          &bytes,
                          nullptr,
                          nullptr);

    if (result == SOCKET_ERROR) {
        ErrorFromSocket();

        *target = nullptr;

        return false;
    }

    return true;
}

Error *argon::vm::io::socket::ErrorNewFromSocket() {
    LPSTR estr;
    String *astr;
    Integer *ecode;
    Dict *eaux;

    int err = WSAGetLastError();
    int length = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                               nullptr, err, 0,
                               (LPSTR) &estr, 0, nullptr);

    if (length == 0) {
        ErrorFormat(kOSError[0], "unable to obtain error message");
        return nullptr;
    }

    // Remove space at the end of the error string
    if ((astr = StringNew(estr, length - 1)) == nullptr) {
        LocalFree(estr);
        return nullptr;
    }

    LocalFree(estr);

    if ((ecode = IntNew(err)) == nullptr) {
        Release(astr);
        return nullptr;
    }

    if ((eaux = DictNew()) == nullptr) {
        Release(astr);
        Release(ecode);
        return nullptr;
    }

    if (!DictInsert(eaux, "code", (ArObject *) ecode)) {
        Release(astr);
        Release(eaux);
        Release(ecode);
        return nullptr;
    }

    Release(ecode);

    auto *ret = ErrorNew(kWSAError[0], astr, eaux);

    Release(astr);
    Release(eaux);

    return ret;
}

Socket *argon::vm::io::socket::Dup(const Socket *sock) {
    _WSAPROTOCOL_INFOW info{};
    Socket *ret;
    SockHandle handle;

    if (WSADuplicateSocketW(sock->sock, GetCurrentProcessId(), &info) != 0) {
        ErrorNewFromSocket();
        return nullptr;
    }

    handle = WSASocketW(FROM_PROTOCOL_INFO,
                        FROM_PROTOCOL_INFO,
                        FROM_PROTOCOL_INFO,
                        &info,
                        0,
                        WSA_FLAG_NO_HANDLE_INHERIT);

    if (handle == INVALID_SOCKET) {
        ErrorNewFromSocket();
        return nullptr;
    }

    if ((ret = SocketNew(sock->family, sock->type, sock->protocol, handle)) == nullptr) {
        closesocket(handle);

        return nullptr;
    }

    return ret;
}

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol) {
    Socket *sock;
    SockHandle handle;

    if ((handle = WSASocketW(domain, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT)) ==
        INVALID_SOCKET) {
        ErrorFromSocket();
        return nullptr;
    }

    if ((sock = SocketNew(domain, type, protocol, handle)) == nullptr) {
        closesocket(handle);
        return nullptr;
    }

    return sock;
}

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol, SockHandle handle) {
    Socket *sock;

    if (!AddHandle(EvLoopGet(), (EvHandle) handle)) {
        closesocket(handle);
        return nullptr;
    }

    if ((sock = MakeObject<Socket>(type_socket_)) == nullptr) {
        closesocket(handle);
        return nullptr;
    }

    sock->sock = handle;

    sock->family = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->timeout = 0;

    sock->AcceptEx = nullptr;
    sock->ConnectEx = nullptr;

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