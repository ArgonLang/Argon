// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

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

#include <mutex>

#include <object/arobject.h>
#include <object/datatype/integer.h>

#include <module/modules.h>

#include "object/datatype/error.h"

#include "object/datatype/tuple.h"
#include "object/datatype/list.h"
#include "vm/runtime.h"
#include "object/datatype/bytes.h"
#include "socket.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::socket;

ARGON_FUNCTION5(socket_, create, "Create a new socket using the given address family, socket type and protocol number."
                                 ""
                                 "- Parameters:"
                                 "  - family: family."
                                 "  - type: type."
                                 "  - protocol: protocol."
                                 "- Returns: (socket, err)", 3, false) {
    Socket *sock;

    if (!CheckArgs("i:family,i:type,i:protocol", func, argv, count, type_string_))
        return nullptr;

    sock = socket::SocketNew((int) ((Integer *) argv[0])->integer,
                             (int) ((Integer *) argv[1])->integer,
                             (int) ((Integer *) argv[2])->integer);

    if (sock == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return TupleReturn(sock, nullptr);
}

Tuple *ParseAddrInfo(addrinfo *info) {
    char saddr[INET6_ADDRSTRLEN];
    const char *canonname = "";
    Tuple *ret = nullptr;
    ArObject *tmp;

    if (info->ai_canonname != nullptr)
        canonname = info->ai_canonname;

    if (info->ai_addr->sa_family == AF_INET) {
        auto *addr_in = (sockaddr_in *) info->ai_addr;

        if (inet_ntop(info->ai_addr->sa_family, &addr_in->sin_addr, saddr, INET6_ADDRSTRLEN) == nullptr)
            return (Tuple *) ErrorSetFromSocket();

        tmp = TupleNew("si", saddr, ntohs(addr_in->sin_port));
    } else if (info->ai_addr->sa_family == AF_INET6) {
        auto *addr_in = (sockaddr_in6 *) info->ai_addr;

        if (inet_ntop(info->ai_addr->sa_family, &addr_in->sin6_addr, saddr, INET6_ADDRSTRLEN) == nullptr)
            return (Tuple *) ErrorSetFromSocket();

        tmp = TupleNew("sHII", saddr, ntohs(addr_in->sin6_port), addr_in->sin6_flowinfo, addr_in->sin6_scope_id);
    } else
        return (Tuple *) ErrorFormat(type_value_error_, "'%d' unsupported family", info->ai_addr->sa_family);

    if (tmp != nullptr) {
        ret = TupleNew("iiisa", info->ai_family, info->ai_socktype, info->ai_protocol, canonname, tmp);
        Release(tmp);
    }

    return ret;
}

ARGON_FUNCTION5(socket_, getaddrinfo, "Translate the host/port argument into a sequence of 5-tuple that contain "
                                      "all the necessary arguments for creating a socket connected to that service."
                                      ""
                                      "5-tuples format: (family, type, proto, canonname, sockaddr)."
                                      ""
                                      "- Parameters:"
                                      "  - name: host name."
                                      "  - service: service."
                                      "  - family: af family."
                                      "  - type: type"
                                      "  - flags: flags"
                                      "- Returns:"
                                      "Returns: (addrinfo, err)", 5, false) {
    addrinfo hints{};
    const char *service = nullptr;
    addrinfo *result;
    List *ret;
    int retval;

    if (!CheckArgs("s:name,s?:service,i:family,i:type,i:flags", func, argv, count, type_string_))
        return nullptr;

    if (!IsNull(argv[1]))
        service = (const char *) ((String *) argv[1])->buffer;

    hints.ai_family = (int) ((Integer *) argv[2])->integer;
    hints.ai_socktype = (int) ((Integer *) argv[3])->integer;
    hints.ai_flags = (int) ((Integer *) argv[4])->integer;

    if ((retval = getaddrinfo((const char *) ((String *) argv[0])->buffer, service, &hints, &result)) != 0) {
        return ARGON_OBJECT_TUPLE_ERROR(ErrorFormatNoPanic(type_gai_error_, "%s", gai_strerror(retval)));
    }

    if ((ret = ListNew()) == nullptr) {
        freeaddrinfo(result);
        return nullptr;
    }

    for (addrinfo *cur = result; cur != nullptr; cur = cur->ai_next) {
        auto info = ParseAddrInfo(cur);

        if (info == nullptr || !ListAppend(ret, info)) {
            Release(info);
            Release(ret);
            return nullptr;
        }

        Release(info);
    }

    freeaddrinfo(result);

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, getnameinfo, "Translate a socket address sockaddr into a 2-tuple."
                                      ""
                                      "2-tuple format: (address, port)."
                                      ""
                                      "- Parameters:"
                                      "  - sockaddr: address."
                                      "  - flags: flags."
                                      "- Returns: (nameinfo, err)", 2, false) {
    sockaddr_in addr_in{};
    sockaddr_in6 addr6_in{};
    auto *addr_ptr = (sockaddr *) &addr_in;
    const char *saddr = nullptr;
    char *hbuf;
    char *sbuf;
    Tuple *ret;

    bool ok = false;

    if (!CheckArgs("t:sockaddr,i:flags", func, argv, count, type_string_))
        return nullptr;

    addr_ptr->sa_family = AF_INET;
    if (((Tuple *) argv[0])->len == 4) {
        if (!(ok = TupleUnpack((Tuple *) argv[0], "sHII", &saddr, &addr6_in.sin6_port,
                               &addr6_in.sin6_flowinfo, &addr6_in.sin6_scope_id))) {
            if (inet_pton(AF_INET6, saddr, &addr6_in.sin6_addr) <= 0)
                return ARGON_OBJECT_TUPLE_ERROR(ErrorFormatNoPanic(type_value_error_, "getnameinfo: illegal address"));
        }
        addr6_in.sin6_port = htons(addr6_in.sin6_port);
        addr_ptr = (sockaddr *) &addr6_in;
        addr_ptr->sa_family = AF_INET6;
    } else if (((Tuple *) argv[0])->len == 2) {
        if ((ok = TupleUnpack((Tuple *) argv[0], "sH", &saddr, &addr_in.sin_port))) {
            if (inet_pton(AF_INET, saddr, &addr_in.sin_addr) <= 0)
                return ARGON_OBJECT_TUPLE_ERROR(ErrorFormatNoPanic(type_value_error_, "getnameinfo: illegal address"));
        }
        addr_in.sin_port = htons(addr_in.sin_port);
    }

    if (!ok)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorFormatNoPanic(type_type_error_, "getnameinfo: illegal sockaddr argument"));

    if ((hbuf = ArObjectNewRaw<char *>(NI_MAXHOST + NI_MAXSERV)) == nullptr)
        return nullptr;

    sbuf = hbuf + NI_MAXHOST;

    if (getnameinfo(addr_ptr, sizeof(sockaddr), hbuf,
                    NI_MAXHOST, sbuf, NI_MAXSERV, (int) ((Integer *) argv[1])->integer) != 0) {
        argon::memory::Free(hbuf);
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());
    }

    ret = TupleNew("ss", hbuf, sbuf);
    argon::memory::Free(hbuf);

    return TupleReturn(ret, nullptr);
}

Tuple *ParseProtoEnt(protoent *pent) {
    List *aliases;
    Tuple *t_aliases;
    Tuple *ret;

    if ((aliases = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = pent->p_aliases; *cursor != nullptr; cursor++) {
        String *tmp = StringNew(*cursor);
        if (tmp == nullptr) {
            Release(aliases);
            return nullptr;
        }

        if (!ListAppend(aliases, tmp)) {
            Release(tmp);
            Release(aliases);
            return nullptr;
        }

        Release(tmp);
    }

    if ((t_aliases = TupleNew(aliases)) == nullptr) {
        Release(aliases);
        return nullptr;
    }

    Release(aliases);

    ret = TupleNew("sai", pent->p_name, t_aliases, pent->p_proto);
    Release(t_aliases);

    return ret;
}

ARGON_FUNCTION5(socket_, getprotobyname, "Translate an internet protocol name."
                                         ""
                                         "2-tuple format: (name, (alias...), id)"
                                         ""
                                         "- Parameter name: name."
                                         "- Returns: (protoid, err)", 1, false) {
    Tuple *ret;
    protoent *pent;

    if (!CheckArgs("s:name", func, argv, count, type_string_))
        return nullptr;

    if ((pent = getprotobyname((const char *) ((String *) argv[0])->buffer)) == nullptr) {
        return ARGON_OBJECT_TUPLE_ERROR(
                ErrorFormatNoPanic(type_os_error_, "protocol '%s' not found", ((String *) argv[0])->buffer));
    }

    if ((ret = ParseProtoEnt(pent)) == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, getprotobynumber, "Translate an internet protocol number to related name."
                                           ""
                                           "2-tuple format: (name, (alias...), id)"
                                           ""
                                           "- Parameter number: proto."
                                           "- Returns: (protoname, err)", 1, false) {
    Tuple *ret;
    protoent *pent;

    if (!CheckArgs("i:proto", func, argv, count, type_string_))
        return nullptr;

    if ((pent = getprotobynumber((int) ((Integer *) argv[0])->integer)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(
                ErrorFormatNoPanic(type_os_error_, "protocol '%d' not found", ((Integer *) argv[0])->integer));

    if ((ret = ParseProtoEnt(pent)) == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

Tuple *ParseSrvEnt(servent *sent) {
    List *aliases;
    Tuple *t_aliases;
    Tuple *ret;

    if ((aliases = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = sent->s_aliases; *cursor != nullptr; cursor++) {
        String *tmp = StringNew(*cursor);
        if (tmp == nullptr) {
            Release(aliases);
            return nullptr;
        }

        if (!ListAppend(aliases, tmp)) {
            Release(tmp);
            Release(aliases);
            return nullptr;
        }

        Release(tmp);
    }

    if ((t_aliases = TupleNew(aliases)) == nullptr) {
        Release(aliases);
        return nullptr;
    }

    Release(aliases);

    ret = TupleNew("saHs", sent->s_name, t_aliases, ntohs(sent->s_port), sent->s_proto);
    Release(t_aliases);

    return ret;
}

ARGON_FUNCTION5(socket_, getservbyname,
                "Translate an internet service name and protocol name to a port number for that service."
                ""
                "4-tuple format: (name, (alias...), port, protocol)"
                ""
                "- Parameters:"
                "  - name: service name."
                "  - proto: protocol name."
                "- Returns: (service, err)", 2, false) {
    const char *pname = nullptr;
    ArObject *err;
    ArObject *ret;
    servent *sent;

    if (!CheckArgs("s:name,s?:proto", func, argv, count, type_string_))
        return nullptr;

    if (!IsNull(argv[1]))
        pname = (const char *) ((String *) argv[1])->buffer;

    if ((sent = getservbyname((const char *) ((String *) argv[0])->buffer, pname)) == nullptr) {
        if (pname != nullptr)
            err = ErrorFormatNoPanic(type_os_error_, "service '%s' for protocol: '%s' not found",
                                     ((String *) argv[0])->buffer, pname);
        else
            err = ErrorFormatNoPanic(type_os_error_, "service '%s' not found", ((String *) argv[0])->buffer);

        return ARGON_OBJECT_TUPLE_ERROR(err);
    }

    if ((ret = ParseSrvEnt(sent)) == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, getservbyport,
                "Translate an internet port number and protocol name to a service name for that service."
                ""
                "4-tuple format: (name, (alias...), port, protocol)"
                ""
                "- Parameters:"
                "  - port: port number."
                "  - proto: protocol name."
                "- Returns: (service, err)", 2, false) {
    const char *pname = nullptr;
    ArObject *err;
    ArObject *ret;
    servent *sent;

    if (!CheckArgs("i:port,s?:proto", func, argv, count, type_string_))
        return nullptr;

    if (!IsNull(argv[1]))
        pname = (const char *) ((String *) argv[1])->buffer;

    if ((sent = getservbyport((int) htons(((Integer *) argv[0])->integer), pname)) == nullptr) {
        if (pname != nullptr)
            err = ErrorFormatNoPanic(type_os_error_, "service '%s' for protocol: '%s' not found",
                                     ((String *) argv[0])->buffer, pname);
        else
            err = ErrorFormatNoPanic(type_os_error_, "service '%s' not found", ((String *) argv[0])->buffer);

        return ARGON_OBJECT_TUPLE_ERROR(err);
    }

    if ((ret = ParseSrvEnt(sent)) == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, gethostname, "Get machine hostname."
                                      ""
                                      "- Returns: string containing the hostname of the machine where "
                                      "the Argon is currently executing.", 0, false) {
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   256
#define NO_HOST_NAME_MAX
#endif
    String *ret;
    char *hname;

    if ((hname = ArObjectNewRaw<char *>(HOST_NAME_MAX)) == nullptr)
        return nullptr;

    if (gethostname(hname, HOST_NAME_MAX) < 0) {
        argon::memory::Free(hname);
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());
    }

    ret = StringNew(hname);
    argon::memory::Free(hname);

    return TupleReturn(ret, nullptr);
#ifdef NO_HOST_NAME_MAX
#undef HOST_NAME_MAX
#undef NO_HOST_NAME_MAX
#endif
}

ARGON_FUNCTION5(socket_, gethostbyaddr, "Return a triple contains hostname and other info."
                                        ""
                                        "3-tuple format: (hostname, aliaslist, ipaddrlist)"
                                        ""
                                        "- Returns: (host, err)", 1, false) {
    static std::mutex mtx;
    char saddr[INET_ADDRSTRLEN]{};
    in_addr addr_in{};
    hostent *hent;
    String *tmp;
    List *names;
    List *addrs;
    Tuple *ret;

    std::unique_lock lock(mtx);

    if (!CheckArgs("s:ip_string", func, argv, count, type_string_))
        return nullptr;

    if (inet_pton(AF_INET, (const char *) ((String *) argv[0])->buffer, &addr_in.s_addr) <= 0)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorFormatNoPanic(type_value_error_, "gethostbyaddr: illegal address"));

#ifdef _ARGON_PLATFORM_WINDOWS
    if ((hent = gethostbyaddr((char *) &addr_in, sizeof(in_addr), AF_INET)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());
#else
    if ((hent = gethostbyaddr(&addr_in, sizeof(in_addr), AF_INET)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());
#endif

    if ((names = ListNew()) == nullptr)
        return nullptr;

    for (char **cursor = hent->h_aliases; *cursor != nullptr; cursor++) {
        if ((tmp = StringNew(*cursor)) == nullptr) {
            Release(names);
            return nullptr;
        }

        if (!ListAppend(names, tmp)) {
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

        if (!ListAppend(addrs, tmp)) {
            Release(tmp);
            Release(names);
            Release(addrs);
            return nullptr;
        }

        Release(tmp);
    }

    ret = TupleNew("saa", hent->h_name, names, addrs);
    Release(names);
    Release(addrs);

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, ntohl, "Convert 32-bit positive integers from network to host byte order."
                                ""
                                "- Parameter number: number."
                                "- Returns: 32-bit positive integer in host byte order.", 1, false) {
    if (!CheckArgs("i:num", func, argv, count, type_string_))
        return nullptr;

    return IntegerNew(ntohl(((Integer *) argv[0])->integer));
}

ARGON_FUNCTION5(socket_, ntohs, "Convert 16-bit positive integers from network to host byte order."
                                ""
                                "- Parameter number: number."
                                "- Returns: 16-bit positive integer in host byte order.", 1, false) {
    if (!CheckArgs("i:num", func, argv, count, type_string_))
        return nullptr;

    return IntegerNew(ntohs((unsigned short) ((Integer *) argv[0])->integer));
}

ARGON_FUNCTION5(socket_, htonl, "Convert 32-bit positive integers from host to network byte order."
                                ""
                                "- Parameter number: number."
                                "- Returns: 32-bit positive integer in network byte order.", 1, false) {
    if (!CheckArgs("i:num", func, argv, count, type_string_))
        return nullptr;

    return IntegerNew(htonl(((Integer *) argv[0])->integer));
}

ARGON_FUNCTION5(socket_, htons, "Convert 16-bit positive integers from host to network byte order."
                                ""
                                "- Parameter number: number."
                                "- Returns: 16-bit positive integer in network byte order.", 1, false) {
    if (!CheckArgs("i:num", func, argv, count, type_string_))
        return nullptr;

    return IntegerNew(htons((unsigned short) ((Integer *) argv[0])->integer));
}

ARGON_FUNCTION5(socket_, pton, "Convert an IP address from its family-specific string format to a packed, binary format."
                               ""
                               "- Parameters:"
                               "  - af: address family."
                               "  - ip_string: IP address."
                               "- Returns: (packed_ip, err)", 2, false) {
    in6_addr addr{};
    Bytes *ret;
    const char *saddr;

    int sa_family;

    if (!CheckArgs("i:af,s:ip_string", func, argv, count, type_string_))
        return nullptr;

    sa_family = (int) ((Integer *) argv[0])->integer;
    saddr = (const char *) ((String *) argv[1])->buffer;

    if (inet_pton(sa_family, saddr, &addr) <= 0)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());

    ret = BytesNew((unsigned char *) &addr, sa_family == AF_INET ? sizeof(in_addr) : sizeof(in6_addr), true);
    if (ret == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

ARGON_FUNCTION5(socket_, ntop, "Convert an IP address from binary format to family-specific string representation."
                               ""
                               "- Parameters:"
                               "  - af: address family."
                               "  - packed_ip: packed ip address."
                               "- Returns: (packed_ip, err)", 2, false) {
    char saddr[INET6_ADDRSTRLEN];
    ArBuffer buffer{};
    String *ret;
    int sa_family;

    if (!CheckArgs("i:af,x:packed_ip", func, argv, count, type_string_))
        return nullptr;

    sa_family = (int) ((Integer *) argv[0])->integer;

    if (!BufferGet(argv[1], &buffer, ArBufferFlags::READ))
        return nullptr;

    if (inet_ntop(sa_family, buffer.buffer, saddr, INET6_ADDRSTRLEN) == nullptr) {
        BufferRelease(&buffer);
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());
    }

    BufferRelease(&buffer);

    if ((ret = StringNew(saddr)) == nullptr)
        return nullptr;

    return TupleReturn(ret, nullptr);
}

bool SocketInit(Module *module) {
#define AddIntConstant(c)                       \
    if(!ModuleAddIntConstant(module, #c, c))    \
        goto ERROR

#ifdef _ARGON_PLATFORM_WINDOWS
    WSADATA WSAData;
    int err;
    err = WSAStartup(0x0101, &WSAData);

    switch (err) {
        case 0:
            break;
        case WSASYSNOTREADY:
            ErrorFormat(type_os_error_, "WSAStartup failed: network not ready");
            return false;
        case WSAVERNOTSUPPORTED:
        case WSAEINVAL:
            ErrorFormat(type_os_error_, "WSAStartup failed: requested version not supported");
            return false;
        default:
            ErrorFormat(type_os_error_, "WSAStartup failed: error code %d", err);
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

    if (TypeInit((TypeInfo *) socket::type_socket_, nullptr))
        return true;

    ERROR:
#ifdef _ARGON_PLATFORM_WINDOWS
    WSACleanup();
#endif
    return false;

#undef AddIntConstant
}

void SocketFinalize(Module *module) {
#ifdef _ARGON_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

const PropertyBulk socket_bulk[] = {
        MODULE_EXPORT_TYPE(type_socket_),
        MODULE_EXPORT_FUNCTION(socket_create_),
        MODULE_EXPORT_FUNCTION(socket_getaddrinfo_),
        MODULE_EXPORT_FUNCTION(socket_getnameinfo_),
        MODULE_EXPORT_FUNCTION(socket_getprotobyname_),
        MODULE_EXPORT_FUNCTION(socket_getprotobynumber_),
        MODULE_EXPORT_FUNCTION(socket_getservbyname_),
        MODULE_EXPORT_FUNCTION(socket_getservbyport_),
        MODULE_EXPORT_FUNCTION(socket_gethostname_),
        MODULE_EXPORT_FUNCTION(socket_gethostbyaddr_),
        MODULE_EXPORT_FUNCTION(socket_htonl_),
        MODULE_EXPORT_FUNCTION(socket_htons_),
        MODULE_EXPORT_FUNCTION(socket_ntohl_),
        MODULE_EXPORT_FUNCTION(socket_ntohs_),
        MODULE_EXPORT_FUNCTION(socket_pton_),
        MODULE_EXPORT_FUNCTION(socket_ntop_),
        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_socket = {
        "_socket",
        "Module socket provides access to the socket interface.",
        socket_bulk,
        SocketInit,
        SocketFinalize
};
const argon::object::ModuleInit *argon::module::module_socket_ = &module_socket;
