// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>

#undef CONST
#undef FASTCALL
#undef Yield

#include <vm/runtime.h>

#include <vm/loop/evloop.h>

#include <vm/datatype/dict.h>
#include <vm/datatype/integer.h>

#include "socket.h"

using namespace argon::vm::loop;
using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

// Prototypes

bool LoadWSAExtension(SOCKET socket, GUID guid, void **target);

// EOL

bool AcceptCallBack(Event *event) {
    argon::vm::ReplaceFrameTopValue(event->aux);

    return true;
}

bool ConnectCallBack(Event *event) {
    argon::vm::ReplaceFrameTopValue(event->initiator);

    return true;
}

bool RecvCallBack(Event *event) {
    auto *bytes = BytesNewHoldBuffer(
            (unsigned char *) event->buffer.wsa.buf,
            event->buffer.allocated,
            event->buffer.wsa.len,
            true);

    if (bytes == nullptr) {
        argon::vm::memory::Free(event->buffer.wsa.buf);
        return false;
    }

    argon::vm::ReplaceFrameTopValue((ArObject *) bytes);

    Release(bytes);

    return true;
}

bool RecvIntoCallBack(Event *event) {
    BufferRelease(&event->buffer.arbuf);

    auto *bytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
    if (bytes != nullptr) {
        argon::vm::ReplaceFrameTopValue((ArObject *) bytes);

        Release(bytes);

        return true;
    }

    return false;
}

bool SendCallBack(Event *event) {
    BufferRelease(&event->buffer.arbuf);

    auto *wbytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
    if (wbytes != nullptr) {
        argon::vm::ReplaceFrameTopValue((ArObject *) wbytes);

        Release(wbytes);

        return true;
    }

    return false;
}

bool argon::vm::io::socket::Accept(Socket *sock) {
    constexpr GUID wsacceptex = WSAID_ACCEPTEX;
    Socket *remote;

    if (sock->AcceptEx == nullptr && !LoadWSAExtension(sock->sock, wsacceptex, (void **) &sock->AcceptEx))
        return false;

    if ((remote = SocketNew(sock->family, sock->type, sock->protocol)) == nullptr)
        return false;

    auto *ovr = EventNew(GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = AcceptCallBack;

    ovr->aux = (ArObject *) remote;

    auto result = sock->AcceptEx(
            sock->sock,
            remote->sock,
            &remote->addr,
            0,
            0,
            sizeof(sockaddr_storage),
            nullptr,
            (OVERLAPPED *) ovr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(ovr);

        ErrorFromSocket();

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
    constexpr GUID wconnectex = WSAID_CONNECTEX;
    sockaddr_in local{};

    if (sock->ConnectEx == nullptr && !LoadWSAExtension(sock->sock, wconnectex, (void **) &sock->ConnectEx))
        return false;

    // Bind socket
    local.sin_family = (short) sock->family;
    local.sin_addr.S_un.S_addr = INADDR_ANY;

    if (!Bind(sock, (const struct sockaddr *) &local, sizeof(sockaddr_in)))
        return false;

    auto *ovr = EventNew(GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = ConnectCallBack;

    auto result = sock->ConnectEx(sock->sock, addr, len, nullptr, 0, nullptr, (OVERLAPPED *) ovr);
    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(ovr);

        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::Listen(const Socket *sock, int backlog) {
    return listen(sock->sock, backlog) == 0;
}

bool argon::vm::io::socket::Recv(Socket *sock, size_t len, int flags) {
    Event *ovr;

    if ((ovr = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if ((ovr->buffer.wsa.buf = (char *) memory::Alloc(len)) == nullptr) {
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = (u_long) len;
    ovr->buffer.allocated = len;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = RecvCallBack;

    auto result = WSARecv(sock->sock,
                          &ovr->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &flags,
                          ovr,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        memory::Free(ovr->buffer.wsa.buf);

        vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(ovr);

        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags) {
    Event *ovr = EventNew(GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

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

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = RecvIntoCallBack;

    auto result = WSARecv(sock->sock,
                          &ovr->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &flags,
                          ovr,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&ovr->buffer.arbuf);

        vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(ovr);

        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, int flags) {
    Event *ovr;

    if((ovr = EventNew(GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if (!BufferGet(buffer, &ovr->buffer.arbuf, BufferFlags::READ)) {
        EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.buf = (char *) ovr->buffer.arbuf.buffer;
    ovr->buffer.wsa.len = (u_long) ovr->buffer.arbuf.length;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = SendCallBack;

    auto result = WSASend(sock->sock,
                          &ovr->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD) flags,
                          ovr,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&ovr->buffer.arbuf);

        vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(ovr);

        ErrorFromSocket();

        return false;
    }

    return true;
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

Socket *argon::vm::io::socket::SocketNew(int domain, int type, int protocol) {
    Socket *sock;
    SockHandle handle;

    if ((handle = WSASocketW(domain, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT)) ==
        INVALID_SOCKET) {
        ErrorFromSocket();
        return nullptr;
    }

    if (!EventLoopAddHandle(GetEventLoop(), (EvHandle) handle)) {
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

    sock->AcceptEx = nullptr;
    sock->ConnectEx = nullptr;

    return sock;
}

#endif