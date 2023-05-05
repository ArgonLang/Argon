// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#endif

#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/tuple.h>

#include <argon/vm/io/socket/socket.h>

#include <argon/vm/mod/modules.h>

#undef CONST
#undef ERROR
#undef PUBLIC

using namespace argon::vm::io::socket;
using namespace argon::vm::datatype;

// Prototypes

Tuple *ParseAddrInfo(addrinfo *info);

Tuple *ParseProtoEnt(protoent *pent);

Tuple *ParseSrvEnt(servent *sent);

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

ARGON_FUNCTION(socket_gethostname, gethostname,
               "Get machine hostname.\n"
               "\n"
               "- Returns: String containing the hostname of the machine.\n",
               nullptr, false, false) {
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   256
#define NO_HOST_NAME_MAX
#endif
    char *hname;

    if ((hname = (char *) argon::vm::memory::Alloc(HOST_NAME_MAX)) == nullptr)
        return nullptr;

    if (gethostname(hname, HOST_NAME_MAX) < 0) {
        argon::vm::memory::Free(hname);
        ErrorFromSocket();
        return nullptr;
    }

    auto *ret = (ArObject *) StringNew(hname);

    argon::vm::memory::Free(hname);

    return ret;
#ifdef NO_HOST_NAME_MAX
#undef HOST_NAME_MAX
#undef NO_HOST_NAME_MAX
#endif
}

ARGON_FUNCTION(socket_gethostbyaddr, gethostbyaddr,
               "Return a triple contains hostname and other info.\n"
               "\n"
               "3-tuple format: (hostname, aliaslist, ipaddrlist).\n"
               "\n"
               "- Parameters address: Address.\n"
               "- Returns: (hostname, aliaslist, ipaddrlist).\n",
               "s: address", false, false) {
    static std::mutex mtx;

    char saddr[INET_ADDRSTRLEN]{};
    in_addr addr_in{};

    hostent *hent;
    List *names;
    List *addrs;
    String *tmp;

    std::unique_lock lock(mtx);

    if (inet_pton(AF_INET, (const char *) ARGON_RAW_STRING((String *) args[0]), &addr_in.s_addr) <= 0) {
        ErrorFormat(kValueError[0], "gethostbyaddr: illegal address");
        return nullptr;
    }

#ifdef _ARGON_PLATFORM_WINDOWS
    if ((hent = gethostbyaddr((char *) &addr_in, sizeof(in_addr), AF_INET)) == nullptr) {
        ErrorFromSocket();
        return nullptr;
    }
#else
    if ((hent = gethostbyaddr(&addr_in, sizeof(in_addr), AF_INET)) == nullptr) {
        ErrorFromSocket();
        return nullptr;
    }
#endif

    if ((names = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = hent->h_aliases; *cursor != nullptr; cursor++) {
        if ((tmp = StringNew(*cursor)) == nullptr) {
            Release(names);
            return nullptr;
        }

        if (!ListAppend(names, (ArObject *) tmp)) {
            Release(tmp);
            Release(names);
            return nullptr;
        }

        Release(tmp);
    }

    if ((addrs = ListNew()) == nullptr) {
        Release(names);
        return nullptr;
    }

    for (auto **cursor = (in_addr **) hent->h_addr_list; *cursor != nullptr; cursor++) {
        if (inet_ntop(AF_INET, &(*cursor)->s_addr, saddr, INET_ADDRSTRLEN) == nullptr) {
            Release(names);
            Release(addrs);
            return nullptr;
        }

        if ((tmp = StringNew(saddr)) == nullptr) {
            Release(names);
            Release(addrs);
            return nullptr;
        }

        if (!ListAppend(addrs, (ArObject *) tmp)) {
            Release(tmp);
            Release(names);
            Release(addrs);
            return nullptr;
        }

        Release(tmp);
    }

    auto *t_names = TupleConvertList(&names);
    auto t_addrs = TupleConvertList(&addrs);

    Release(names);
    Release(addrs);

    auto *ret = (ArObject *) TupleNew("soo", hent->h_name, names, addrs);

    Release(t_names);
    Release(t_addrs);

    return ret;
}

ARGON_FUNCTION(socket_getnameinfo, getnameinfo,
               "Translate a socket address sockaddr into a 2-tuple\n."
               "\n"
               "2-tuple format: (address, port).\n"
               "\n"
               "- Parameters:\n"
               "  - sockaddr: Address.\n"
               "  - flags: Flags.\n"
               "- Returns: (address, port).\n",
               "t: sockaddr, i: flags", false, false) {
    sockaddr_storage addr_in{};
    auto *addr_ptr = (sockaddr *) &addr_in;
    const char *saddr = nullptr;
    char *hbuf;
    char *sbuf;

    bool ok = false;

    addr_ptr->sa_family = AF_INET;
    if (((Tuple *) args[0])->length == 4) {
        ok = TupleUnpack((Tuple *) args[0], "shII", &saddr, &((sockaddr_in6 *) &addr_in)->sin6_port,
                         &((sockaddr_in6 *) &addr_in)->sin6_flowinfo, &((sockaddr_in6 *) &addr_in)->sin6_scope_id);

        if (ok && inet_pton(AF_INET6, saddr, &((sockaddr_in6 *) &addr_in)->sin6_addr) <= 0) {
            ErrorFormat(kValueError[0], "getnameinfo: illegal address");
            return nullptr;
        }

        ((sockaddr_in6 *) &addr_in)->sin6_port = htons(((sockaddr_in6 *) &addr_in)->sin6_port);
        addr_ptr->sa_family = AF_INET6;
    } else if (((Tuple *) args[0])->length == 2) {
        ok = TupleUnpack((Tuple *) args[0], "sh", &saddr, &((sockaddr_in *) &addr_in)->sin_port);
        if (ok && inet_pton(AF_INET, saddr, &((sockaddr_in *) &addr_in)->sin_addr) <= 0) {
            ErrorFormat(kValueError[0], "getnameinfo: illegal address");
            return nullptr;
        }

        ((sockaddr_in *) &addr_in)->sin_port = htons(((sockaddr_in *) &addr_in)->sin_port);
    }

    if (!ok) {
        ErrorFormat(kTypeError[0], "getnameinfo: illegal sockaddr argument");
        return nullptr;
    }

    if ((hbuf = (char *) argon::vm::memory::Alloc(NI_MAXHOST + NI_MAXSERV)) == nullptr)
        return nullptr;

    sbuf = hbuf + NI_MAXHOST;

    if (getnameinfo(addr_ptr, sizeof(sockaddr), hbuf, NI_MAXHOST, sbuf, NI_MAXSERV,
                    (int) ((Integer *) args[1])->sint) != 0) {
        argon::vm::memory::Free(hbuf);

        ErrorFromSocket();

        return nullptr;
    }

    auto *ret = TupleNew("ss", hbuf, sbuf);

    argon::vm::memory::Free(hbuf);

    return (ArObject *) ret;
}


ARGON_FUNCTION(socket_getprotobyname, getprotobyname,
               "Translate an internet protocol name.\n"
               "\n"
               "2-tuple format: (name, (alias...), id).\n"
               "\n"
               "- Parameter name: Name.\n"
               "- Returns: ((name, (alias...), id).\n",
               "s: name", false, false) {
    static std::mutex protobyname_mtx;
    protoent *pent;

    std::unique_lock _(protobyname_mtx);

    if ((pent = getprotobyname((const char *) ARGON_RAW_STRING((String *) args[0]))) == nullptr) {
        ErrorFormat(kOSError[0], "protocol '%s' not found", ARGON_RAW_STRING((String *) args[0]));
        return nullptr;
    }

    return (ArObject *) ParseProtoEnt(pent);
}

ARGON_FUNCTION(socket_getprotobynumber, getprotobynumber,
               "Translate an internet protocol number to related name.\n"
               "\n"
               "2-tuple format: (name, (alias...), id).\n"
               "\n"
               "- Parameter number: Protocol number.\n"
               "- Returns: ((name, (alias...), id).\n",
               "i: number", false, false) {
    static std::mutex protobynumber_mtx;
    protoent *pent;

    std::unique_lock _(protobynumber_mtx);

    if ((pent = getprotobynumber((int) ((Integer *) *args)->sint)) == nullptr) {
        ErrorFormat(kOSError[0], "protocol '%d' not found", ((Integer *) *args)->sint);
        return nullptr;
    }

    return (ArObject *) ParseProtoEnt(pent);
}

ARGON_FUNCTION(socket_getservbyname, getservbyname,
               "Translate an internet service name and protocol name to a port number for that service.\n"
               "\n"
               "4-tuple format: (name, (alias...), port, protocol).\n"
               "\n"
               "- Parameters:\n"
               "  - name: Service name.\n"
               "  - proto: Protocol name.\n"
               "- Returns: (name, (alias...), port, protocol).\n",
               "s: name, sn: proto", false, false) {
    static std::mutex servbyname_mtx;

    const char *pname = nullptr;
    servent *sent;

    if (!IsNull(args[1]))
        pname = (const char *) ARGON_RAW_STRING((String *) args[1]);

    std::unique_lock _(servbyname_mtx);

    if ((sent = getservbyname((const char *) ARGON_RAW_STRING((String *) args[0]), pname)) == nullptr) {
        if (pname != nullptr)
            ErrorFormat(kOSError[0], "service '%s' for protocol: '%s' not found",
                        ARGON_RAW_STRING((String *) args[0]), pname);
        else
            ErrorFormat(kOSError[0], "service '%s' not found",
                        ARGON_RAW_STRING((String *) args[0]));

        return nullptr;
    }

    return (ArObject *) ParseSrvEnt(sent);
}

ARGON_FUNCTION(socket_getservbyport, getservbyport,
               "Translate an internet port number and protocol name to a service name for that service.\n"
               "\n"
               "4-tuple format: (name, (alias...), port, protocol)\n"
               "\n"
               "- Parameters:\n"
               "  - port: Port number.\n"
               "  - proto: Protocol name.\n"
               "- Returns: (name, (alias...), port, protocol).\n",
               "i: port, sn: proto", false, false) {
    static std::mutex servbyport_mtx;
    const char *pname = nullptr;
    servent *sent;

    if (!IsNull(args[1]))
        pname = (const char *) ARGON_RAW_STRING((String *) args[1]);

    std::unique_lock _(servbyport_mtx);

    if ((sent = getservbyport((int) ((Integer *) args[0])->sint, pname)) == nullptr) {
        if (pname != nullptr)
            ErrorFormat(kOSError[0], "service '%s' for protocol: '%s' not found",
                        ARGON_RAW_STRING((String *) args[0]), pname);
        else
            ErrorFormat(kOSError[0], "service '%s' not found",
                        ARGON_RAW_STRING((String *) args[0]));

        return nullptr;
    }

    return (ArObject *) ParseSrvEnt(sent);
}

ARGON_FUNCTION(socket_htonl, htonl,
               "Convert 32-bit positive integers from host to network byte order.\n"
               "\n"
               "- Parameter number: Number.\n"
               "- Returns: 32-bit positive integer in network byte order.\n",
               "i: number", false, false) {

    return (ArObject *) IntNew(htonl((unsigned int) ((Integer *) *args)->sint));
}

ARGON_FUNCTION(socket_htons, htons,
               "Convert 16-bit positive integers from host to network byte order.\n"
               "\n"
               "- Parameter number: Number.\n"
               "- Returns: 16-bit positive integer in network byte order.\n",
               "i: number", false, false) {

    return (ArObject *) IntNew(htons((unsigned short) ((Integer *) *args)->sint));
}

ARGON_FUNCTION(socket_ntohl, ntohl,
               "Convert 32-bit positive integers from network to host byte order.\n"
               "\n"
               "- Parameter number: Number.\n"
               "- Returns: 32-bit positive integer in host byte order.\n",
               "i: number", false, false) {

    return (ArObject *) IntNew(ntohl((unsigned int) ((Integer *) *args)->sint));
}

ARGON_FUNCTION(socket_ntohs, ntohs,
               "Convert 16-bit positive integers from network to host byte order.\n"
               "\n"
               "- Parameter number: Number.\n"
               "- Returns: 16-bit positive integer in host byte order.\n",
               "i: number", false, false) {

    return (ArObject *) IntNew(ntohs((unsigned short) ((Integer *) *args)->sint));
}

ARGON_FUNCTION(socket_ntop, ntop,
               "Convert an IP address from binary format to family-specific string representation.\n"
               "\n"
               "- Parameters:\n"
               "  - packed_ip: Packed ip address.\n"
               "  - af: Address family.\n"
               "- Returns: String address.\n",
               "x: packed_ip, i: af", false, false) {
    char saddr[INET6_ADDRSTRLEN];
    ArBuffer buffer{};
    int sa_family;

    sa_family = (int) ((Integer *) args[1])->sint;

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    if (inet_ntop(sa_family, buffer.buffer, saddr, INET6_ADDRSTRLEN) == nullptr) {
        BufferRelease(&buffer);

        ErrorFromSocket();

        return nullptr;
    }

    BufferRelease(&buffer);

    return (ArObject *) StringNew(saddr);
}

ARGON_FUNCTION(socket_pton, pton,
               "Convert an IP address from its family-specific string format to a packed binary format.\n"
               "\n"
               "- Parameters:\n"
               "  - address: IP address.\n"
               "  - af: Address family.\n"
               "- Returns: Bytes represent a packed IP.\n",
               "s: address, i: af", false, false) {
    in6_addr addr{};

    auto sa_family = (int) ((Integer *) args[1])->sint;
    const auto *saddr = (const char *) ((String *) args[0])->buffer;

    if (inet_pton(sa_family, saddr, &addr) <= 0) {
        ErrorFromSocket();

        return nullptr;
    }

    return (ArObject *) BytesNew((unsigned char *) &addr,
                                 sa_family == AF_INET ? sizeof(in_addr) : sizeof(in6_addr),
                                 true);
}

bool SocketInit(Module *self) {
#define AddIntConstant(c)                   \
    if(!ModuleAddIntConstant(self, #c, c))  \
        goto ERROR

    if (!TypeInit((TypeInfo *) type_socket_, nullptr))
        return false;

#ifdef _ARGON_PLATFORM_WINDOWS
        WSADATA WSAData;

        int err = WSAStartup(MAKEWORD(2, 2), &WSAData);
        switch (err) {
            case 0:
                break;
            case WSASYSNOTREADY:
                ErrorFormat(kOSError[0], "WSAStartup failed: network not ready");
                return false;
            case WSAVERNOTSUPPORTED:
            case WSAEINVAL:
                ErrorFormat(kOSError[0], "WSAStartup failed: requested version not supported");
                return false;
            default:
                ErrorFormat(kOSError[0], "WSAStartup failed: error code %d", err);
                return false;
        }
#endif

#ifdef AF_APPLETALK
    AddIntConstant(AF_APPLETALK);
#endif
    AddIntConstant(AF_INET);
#ifdef AF_INET6
    AddIntConstant(AF_INET6);
#endif
#ifdef AF_NETBIOS
    AddIntConstant(AF_NETBIOS);
#endif
#ifdef AF_UNIX
    AddIntConstant(AF_UNIX);
#endif
#ifdef AF_UNSPEC
    AddIntConstant(AF_UNSPEC);
#endif
#ifdef AF_VSOCK
    AddIntConstant(AF_VSOCK);
#endif

#ifdef PF_APPLETALK
    AddIntConstant(PF_APPLETALK);
#endif
#ifdef PF_INET
    AddIntConstant(PF_INET);
#endif
#ifdef PF_INET6
    AddIntConstant(PF_INET6);
#endif
#ifdef PF_LOCAL
    AddIntConstant(PF_LOCAL);
#endif
#ifdef PF_NETBIOS
    AddIntConstant(PF_NETBIOS);
#endif
#ifdef PF_UNIX
    AddIntConstant(PF_UNIX);
#endif
#ifdef PF_UNSPEC
    AddIntConstant(PF_UNSPEC);
#endif
#ifdef PF_VSOCK
    AddIntConstant(PF_VSOCK);
#endif

    AddIntConstant(SOCK_DGRAM);
#ifdef SOCK_RAW
    AddIntConstant(SOCK_RAW);
#endif
#ifdef SOCK_RDM
    AddIntConstant(SOCK_RDM);
#endif
    AddIntConstant(SOCK_SEQPACKET);
    AddIntConstant(SOCK_STREAM);

#ifdef IPPROTO_IP
    AddIntConstant(IPPROTO_IP);
#endif
#ifdef IPPROTO_IPV4
    AddIntConstant(IPPROTO_IPV4);
#endif
#ifdef IPPROTO_IPV6
    AddIntConstant(IPPROTO_IPV6);
#endif
#ifdef IPPROTO_ICMP
    AddIntConstant(IPPROTO_ICMP);
#endif
#ifdef IPPROTO_ICMPV6
    AddIntConstant(IPPROTO_ICMPV6);
#endif
#ifdef IPPROTO_TCP
    AddIntConstant(IPPROTO_TCP);
#endif
#ifdef IPPROTO_UDP
    AddIntConstant(IPPROTO_UDP);
#endif

    // AddressInfo
    AddIntConstant(AI_ADDRCONFIG);
    AddIntConstant(AI_ALL);
    AddIntConstant(AI_CANONNAME);
    AddIntConstant(AI_V4MAPPED);

    // NameInfo
    AddIntConstant(NI_DGRAM);
    AddIntConstant(NI_NAMEREQD);
    AddIntConstant(NI_NOFQDN);
    AddIntConstant(NI_NUMERICHOST);
    AddIntConstant(NI_NUMERICSERV);

        /* Flags for send, recv */
#ifdef  MSG_OOB
    AddIntConstant(MSG_OOB);
#endif
#ifdef  MSG_PEEK
    AddIntConstant(MSG_PEEK);
#endif
#ifdef  MSG_DONTROUTE
    AddIntConstant(MSG_DONTROUTE);
#endif
#ifdef  MSG_DONTWAIT
    AddIntConstant(MSG_DONTWAIT);
#endif
#ifdef  MSG_EOR
    AddIntConstant(MSG_EOR);
#endif
#ifdef  MSG_TRUNC
    AddIntConstant(MSG_TRUNC);
#endif
#ifdef  MSG_CTRUNC
    AddIntConstant(MSG_CTRUNC);
#endif
#ifdef  MSG_WAITALL
    AddIntConstant(MSG_WAITALL);
#endif
#ifdef  MSG_BTAG
        AddIntConstant(MSG_BTAG);
#endif
#ifdef  MSG_ETAG
        AddIntConstant(MSG_ETAG);
#endif
#ifdef  MSG_NOSIGNAL
    AddIntConstant(MSG_NOSIGNAL);
#endif
#ifdef  MSG_NOTIFICATION
        AddIntConstant(MSG_NOTIFICATION);
#endif
#ifdef  MSG_CMSG_CLOEXEC
        AddIntConstant(MSG_CMSG_CLOEXEC);
#endif
#ifdef  MSG_ERRQUEUE
        AddIntConstant(MSG_ERRQUEUE);
#endif
#ifdef  MSG_CONFIRM
        AddIntConstant(MSG_CONFIRM);
#endif
#ifdef  MSG_MORE
        AddIntConstant(MSG_MORE);
#endif
#ifdef  MSG_EOF
    AddIntConstant(MSG_EOF);
#endif
#ifdef  MSG_BCAST
    AddIntConstant(MSG_BCAST);
#endif
#ifdef  MSG_MCAST
    AddIntConstant(MSG_MCAST);
#endif
#ifdef MSG_FASTOPEN
    AddIntConstant(MSG_FASTOPEN);
#endif

    return true;

    ERROR:
#ifdef _ARGON_PLATFORM_WINDOWS
    WSACleanup();
#endif
    return false;
#undef AddIntConstant
}

const ModuleEntry socket_entries[] = {
        MODULE_EXPORT_TYPE(type_socket_),

        MODULE_EXPORT_FUNCTION(socket_getaddrinfo),
        MODULE_EXPORT_FUNCTION(socket_gethostname),
        MODULE_EXPORT_FUNCTION(socket_gethostbyaddr),
        MODULE_EXPORT_FUNCTION(socket_getnameinfo),
        MODULE_EXPORT_FUNCTION(socket_getprotobyname),
        MODULE_EXPORT_FUNCTION(socket_getprotobynumber),
        MODULE_EXPORT_FUNCTION(socket_getservbyname),
        MODULE_EXPORT_FUNCTION(socket_getservbyport),
        MODULE_EXPORT_FUNCTION(socket_htonl),
        MODULE_EXPORT_FUNCTION(socket_htons),
        MODULE_EXPORT_FUNCTION(socket_ntohl),
        MODULE_EXPORT_FUNCTION(socket_ntohs),
        MODULE_EXPORT_FUNCTION(socket_ntop),
        MODULE_EXPORT_FUNCTION(socket_pton),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleSocket = {
        "argon:socket",
        "Module socket provides access to the socket interface.",
        nullptr,
        socket_entries,
        SocketInit,
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

Tuple *ParseProtoEnt(protoent *pent) {
    List *aliases;
    Tuple *t_aliases;
    Tuple *ret;

    if ((aliases = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = pent->p_aliases; *cursor != nullptr; cursor++) {
        auto *tmp = StringNew(*cursor);
        if (tmp == nullptr) {
            Release(aliases);
            return nullptr;
        }

        if (!ListAppend(aliases, (ArObject *) tmp)) {
            Release(tmp);
            Release(aliases);
            return nullptr;
        }

        Release(tmp);
    }

    t_aliases = TupleConvertList(&aliases);

    Release(aliases);

    if (t_aliases == nullptr)
        return nullptr;

    ret = TupleNew("soi", pent->p_name, t_aliases, pent->p_proto);

    Release(t_aliases);

    return ret;
}

Tuple *ParseSrvEnt(servent *sent) {
    List *aliases;
    Tuple *t_aliases;

    if ((aliases = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = sent->s_aliases; *cursor != nullptr; cursor++) {
        String *tmp = StringNew(*cursor);
        if (tmp == nullptr) {
            Release(aliases);
            return nullptr;
        }

        if (!ListAppend(aliases, (ArObject *) tmp)) {
            Release(tmp);
            Release(aliases);
            return nullptr;
        }

        Release(tmp);
    }

    t_aliases = TupleConvertList(&aliases);

    Release(aliases);

    if (t_aliases == nullptr)
        return nullptr;

    auto *ret = TupleNew("sohs", sent->s_name, t_aliases, ntohs(sent->s_port), sent->s_proto);
    Release(t_aliases);

    return ret;
}