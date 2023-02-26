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
#include <vm/event.h>

#include <vm/datatype/dict.h>
#include <vm/datatype/integer.h>

#include "socket.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

// Prototypes

bool LoadWSAExtension(SOCKET socket, GUID guid, void **target);

// EOL

ArSSize argon::vm::io::socket::Connect(Socket *sock, const sockaddr *addr, socklen_t len) {
    constexpr GUID wconnectex = WSAID_CONNECTEX;
    sockaddr_in local{};

    if (sock->ConnectEx == nullptr && !LoadWSAExtension(sock->sock, wconnectex, (void **) &sock->ConnectEx))
        return -1;

    // Bind socket
    local.sin_family = (short) sock->family;
    local.sin_addr.S_un.S_addr = INADDR_ANY;

    if (!Bind(sock, (const struct sockaddr *) &local, sizeof(sockaddr_in)))
        return -1;

    auto *ovr = vm::EventAlloc(vm::GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return -1;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    auto result = sock->ConnectEx(sock->sock, addr, len, nullptr, 0, nullptr, (OVERLAPPED *) ovr);
    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        vm::SetFiberStatus(FiberStatus::RUNNING);

        vm::EventDel(ovr);

        ErrorFromSocket();
    }

    return -1;
}

ArSSize argon::vm::io::socket::RecvInto(Socket *sock, datatype::ArObject *buffer, int offset, int flags) {
    Event *ovr = vm::EventAlloc(vm::GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return -1;

    if (!BufferGet(buffer, &ovr->buffer.bufferable, BufferFlags::WRITE)) {
        vm::EventDel(ovr);

        return -1;
    }

    if (offset >= ovr->buffer.bufferable.length) {
        ErrorFormat(kOverflowError[0], kOverflowError[2], AR_TYPE_NAME(buffer), ovr->buffer.bufferable.length, offset);

        BufferRelease(&ovr->buffer.bufferable);

        vm::EventDel(ovr);

        return -1;
    }

    ovr->buffer.wsa.buf = (char *) ovr->buffer.bufferable.buffer + offset;
    ovr->buffer.wsa.len = (u_long) ovr->buffer.bufferable.length - offset;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = [](Event *event) {
        BufferRelease(&event->buffer.bufferable);

        auto *bytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
        if (bytes != nullptr) {
            vm::ReplaceFrameTopValue((ArObject *) bytes);

            Release(bytes);
        }
    };

    auto result = WSARecv(sock->sock,
                          &ovr->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD *) &flags,
                          ovr,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&ovr->buffer.bufferable);

        vm::SetFiberStatus(FiberStatus::RUNNING);

        vm::EventDel(ovr);

        ErrorFromSocket();
    }

    return -1;
}

ArSSize argon::vm::io::socket::Send(Socket *sock, datatype::ArObject *buffer, int flags) {
    Event *ovr;

    if((ovr = vm::EventAlloc(vm::GetEventLoop(), (ArObject *) sock)) == nullptr)
        return -1;

    if (!BufferGet(buffer, &ovr->buffer.bufferable, BufferFlags::READ)) {
        vm::EventDel(ovr);

        return -1;
    }

    ovr->buffer.wsa.buf = (char *) ovr->buffer.bufferable.buffer;
    ovr->buffer.wsa.len = (u_long) ovr->buffer.bufferable.length;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = [](Event *event) {
        BufferRelease(&event->buffer.bufferable);

        auto *wbytes = IntNew((IntegerUnderlying) event->buffer.wsa.len);
        if (wbytes != nullptr) {
            vm::ReplaceFrameTopValue((ArObject *) wbytes);

            Release(wbytes);
        }
    };

    auto result = WSASend(sock->sock,
                          &ovr->buffer.wsa,
                          1,
                          nullptr,
                          (DWORD) flags,
                          ovr,
                          nullptr);

    if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
        BufferRelease(&ovr->buffer.bufferable);

        vm::SetFiberStatus(FiberStatus::RUNNING);

        vm::EventDel(ovr);

        ErrorFromSocket();
    }

    return -1;
}

bool argon::vm::io::socket::Accept(Socket *sock, Socket **out) {
    constexpr GUID wsacceptex = WSAID_ACCEPTEX;
    Socket *remote;

    *out = nullptr;

    if (sock->AcceptEx == nullptr && !LoadWSAExtension(sock->sock, wsacceptex, (void **) &sock->AcceptEx))
        return false;

    if ((remote = SocketNew(sock->family, sock->type, sock->protocol)) == nullptr)
        return false;

    auto *ovr = vm::EventAlloc(vm::GetEventLoop(), (ArObject *) sock);
    if (ovr == nullptr)
        return false;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = [](Event *event) {
        vm::ReplaceFrameTopValue(event->aux);
        Release(event->aux);
    };

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

        vm::EventDel(ovr);

        ErrorFromSocket();
    }

    return false;
}

bool argon::vm::io::socket::Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen) {
    if (::bind(sock->sock, addr, addrlen) != 0) {
        ErrorFromSocket();

        return false;
    }

    return true;
}

bool argon::vm::io::socket::Listen(const Socket *sock, int backlog) {
    return listen(sock->sock, backlog) == 0;
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

bool argon::vm::io::socket::Recv(Socket *sock, Bytes **out, size_t len, int flags) {
    Event *ovr;

    *out = nullptr;

    if ((ovr = vm::EventAlloc(vm::GetEventLoop(), (ArObject *) sock)) == nullptr)
        return false;

    if ((ovr->buffer.wsa.buf = (char *) memory::Alloc(len)) == nullptr) {
        vm::EventDel(ovr);

        return false;
    }

    ovr->buffer.wsa.len = (u_long) len;
    ovr->buffer.allocated = len;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    ovr->fiber = vm::GetFiber();

    ovr->callback = [](Event *event) {
        auto *bytes = BytesNewHoldBuffer(
                (unsigned char *) event->buffer.wsa.buf,
                event->buffer.allocated,
                event->buffer.wsa.len,
                true);

        if (bytes == nullptr) {
            memory::Free(event->buffer.wsa.buf);
            return;
        }

        vm::ReplaceFrameTopValue((ArObject *) bytes);

        Release(bytes);
    };

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

        vm::EventDel(ovr);

        ErrorFromSocket();
    }

    return false;
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

    if (!vm::EvLoopRegister(vm::GetEventLoop(), (vm::EvHandle) handle)) {
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