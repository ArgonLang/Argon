// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>

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

    if (sock->ConnectEx != nullptr)
        return true;

    if (!LoadWSAExtension(sock->sock, wconnectex, (void **) &sock->ConnectEx))
        return -1;

    // Bind socket
    local.sin_family = (short) sock->family;
    local.sin_addr.S_un.S_addr = INADDR_ANY;

    if (!Bind(sock, (const struct sockaddr *) &local, sizeof(sockaddr_in)))
        return -1;

    assert(false);
}

bool argon::vm::io::socket::Bind(const Socket *sock, const struct sockaddr *addr, socklen_t addrlen) {
    if (::bind(sock->sock, addr, addrlen) != 0) {
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

    if ((sock = MakeObject<Socket>(type_socket_)) == nullptr) {
        closesocket(handle);
        return nullptr;
    }

    sock->sock = handle;
    sock->family = domain;

    sock->AcceptEx = nullptr;
    sock->ConnectEx = nullptr;

    return sock;
}

#endif