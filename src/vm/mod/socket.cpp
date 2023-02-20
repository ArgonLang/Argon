// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS
#pragma comment(lib, "Ws2_32.lib")

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>

#undef CONST
#undef ERROR
#else

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#endif

#include <vm/datatype/error.h>
#include <vm/datatype/integer.h>
#include <vm/datatype/tuple.h>

#include <vm/io/socket/socket.h>

#include "modules.h"

using namespace argon::vm::io::socket;
using namespace argon::vm::datatype;

// Prototypes

Tuple *ParseAddrInfo(addrinfo *info);

// EOL

ARGON_FUNCTION(socket_getaddrinfo, getaddrinfo,
               "Translate the host/port argument into a sequence of 5-tuple that contain "
               "all the necessary arguments for creating a socket connected to that service.\n"
               "\n"
               "5-tuples format: (family, type, proto, canonname, sockaddr).\n"
               "\n"
               "- Parameters:\n"
               "  - name: Host name.\n"
               "  - service: Service.\n"
               "  - family: AF family.\n"
               "  - type: Type.\n"
               "  - flags: Flags.\n"
               "- Returns: (family, type, proto, canonname, sockaddr).\n",
               "s: name, sn: service, i: family, i: type, i: flags", false, false) {
    addrinfo hints{};
    addrinfo *result;
    const char *service;
    Tuple *ret;
    int retval;

    service = nullptr;
    if (!IsNull(args[1]))
        service = (const char *) ARGON_RAW_STRING((String *) args[1]);

    hints.ai_family = (int) ((Integer *) args[2])->sint;
    hints.ai_socktype = (int) ((Integer *) args[3])->sint;
    hints.ai_flags = (int) ((Integer *) args[4])->sint;

    if ((retval = getaddrinfo((const char *) ARGON_RAW_STRING((String *) args[0]), service, &hints, &result)) != 0) {
        ErrorFormat(kGAIError[0], "%s", gai_strerror(retval));
        return nullptr;
    }

    auto *l_tmp = ListNew();
    if (l_tmp == nullptr) {
        freeaddrinfo(result);
        return nullptr;
    }

    for (addrinfo *cur = result; cur != nullptr; cur = cur->ai_next) {
        auto info = (ArObject *) ParseAddrInfo(cur);

        if (info == nullptr || !ListAppend(l_tmp, info)) {
            Release(info);
            Release(l_tmp);
            return nullptr;
        }

        Release(info);
    }

    freeaddrinfo(result);

    ret = TupleConvertList(&l_tmp);

    Release(l_tmp);

    return (ArObject *) ret;
}

const ModuleEntry socket_entries[] = {
        MODULE_EXPORT_FUNCTION(socket_getaddrinfo),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleSocket = {
        "argon:socket",
        "Module socket provides access to the socket interface.",
        socket_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_socket_ = &ModuleSocket;

Tuple *ParseAddrInfo(addrinfo *info) {
    char saddr[INET6_ADDRSTRLEN];
    const char *canonname = "";
    Tuple *ret = nullptr;
    ArObject *tmp;

    if (info->ai_canonname != nullptr)
        canonname = info->ai_canonname;

    if (info->ai_addr->sa_family == AF_INET) {
        const auto *addr_in = (sockaddr_in *) info->ai_addr;

        if (inet_ntop(info->ai_addr->sa_family, &addr_in->sin_addr, saddr, INET6_ADDRSTRLEN) == nullptr) {
            ErrorFromSocket();
            return nullptr;
        }

        tmp = (ArObject *) TupleNew("sH", saddr, ntohs(addr_in->sin_port));
    } else if (info->ai_addr->sa_family == AF_INET6) {
        const auto *addr_in = (sockaddr_in6 *) info->ai_addr;

        if (inet_ntop(info->ai_addr->sa_family, &addr_in->sin6_addr, saddr, INET6_ADDRSTRLEN) == nullptr) {
            ErrorFromSocket();
            return nullptr;
        }

        tmp = (ArObject *) TupleNew("sHII", saddr, ntohs(addr_in->sin6_port),
                                    addr_in->sin6_flowinfo, addr_in->sin6_scope_id);
    } else {
        ErrorFormat(kValueError[0], "'%d' unsupported family", info->ai_addr->sa_family);
        return nullptr;
    }

    if (tmp != nullptr) {
        ret = TupleNew("iiiso", info->ai_family, info->ai_socktype, info->ai_protocol, canonname, tmp);
        Release(tmp);
    }

    return ret;
}