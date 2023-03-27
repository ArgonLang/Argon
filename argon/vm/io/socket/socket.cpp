// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS
#include <sys/un.h>

#include <argon/vm/loop/evloop.h>
#endif

#include <argon/vm/runtime.h>

#include <argon/vm/io/io.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/io/socket/socket.h>

using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

ARGON_FUNCTION(socket_socket, Socket,
               "Create a new socket using the given address family, socket type and protocol number.\n"
               "\n"
               "- Parameters:\n"
               "  - family: Family.\n"
               "  - type: Type.\n"
               "  - protocol: Protocol.\n"
               "- Returns: Socket object.\n",
               "i: family, i: type, i: protocol", false, false) {
    return (ArObject *) SocketNew((int) ((Integer *) args[0])->sint,
                                  (int) ((Integer *) args[1])->sint,
                                  (int) ((Integer *) args[2])->sint);
}

ARGON_METHOD(socket_accept, accept,
             "Accept a connection.\n"
             "\n"
             "The socket must be bound to an address and listening for connections.\n"
             "\n"
             "- Returns: Socket.\n",
             nullptr, false, false) {
    auto *self = (Socket *) _self;

    Accept(self);

    return nullptr;
}

ARGON_METHOD(socket_bind, bind,
             "Bind the socket to address.\n"
             "\n"
             "The socket must not already be bound.\n"
             "\n"
             "- Parameter address: format of address depends on the address family.\n",
             "st: address", false, false) {
    sockaddr_storage addr{};
    socklen_t addrlen;

    const auto *self = (Socket *) _self;

    if (!AddrToSockAddr(args[0], &addr, &addrlen, self->family))
        return nullptr;

    if (!Bind(self, (const struct sockaddr *) &addr, addrlen))
        return nullptr;

    return IncRef(_self);
}

ARGON_METHOD(socket_connect, connect,
             "Connect to a remote socket at given address.\n"
             "\n"
             "- Parameter address: Format of address depends on the address family.\n",
             "st: address", false, false) {
    sockaddr_storage addr{};
    socklen_t addrlen;

    auto *self = (Socket *) _self;

    if (AddrToSockAddr(args[0], &addr, &addrlen, self->family))
        Connect(self, (const sockaddr *) &addr, addrlen);

    return nullptr;
}

ARGON_METHOD(socket_close, close,
             "Mark the socket closed.\n"
             "\n"
             "The underlying file descriptor is also closed.\n"
             "\n"
             "- Returns: nil.\n",
             nullptr, false, false) {
    if (Close((Socket *) _self))
        return (ArObject *) IncRef(Nil);

    return nullptr;
}

ARGON_METHOD(socket_detach, detach,
             "Put the socket object into closed state.\n"
             "\n"
             "This method does not affect the underlying file descriptor.\n"
             "\n"
             "- Returns: File descriptor as UInt.\n",
             nullptr, false, false) {
    auto *uint = UIntNew(0);

    if (uint != nullptr)
        uint->uint = Detach((Socket *) *args);

    return (ArObject *) uint;
}

ARGON_METHOD(socket_dup, dup,
             "Duplicate the socket.\n"
             "\n"
             "- Returns: Duplicated socket.\n",
             nullptr, false, false) {
    return (ArObject *) Dup((Socket *) *args);
}

ARGON_METHOD(socket_listen, listen,
             "Enable a server to accept connections.\n"
             "\n"
             "Backlog must be at least 0. It specifies the number of unaccepted "
             "connections that the system will allow before refusing new connections.\n"
             "\n"
             "- Parameter backlog: number of unaccepted connections that the system will allow.\n",
             "i: backlog", false, false) {
    const auto *self = (Socket *) _self;

    if (!Listen(self, (int)((Integer *) *args)->sint))
        return nullptr;

    return IncRef(_self);
}

// Inherited from Reader trait
ARGON_METHOD_INHERITED(socket_read, read) {
    auto *self = (Socket *) _self;
    IntegerUnderlying bufsize = ((Integer *) args[0])->sint;

    if (bufsize < 0)
        ErrorFormat(kValueError[0], "size cannot be less than zero");
    else
        Recv(self, bufsize, 0);

    return nullptr;
}

// Inherited from Reader trait
ARGON_METHOD_INHERITED(socket_readinto, readinto) {
    auto *self = (Socket *) _self;
    auto offset = ((Integer *) args[1])->sint;

    if (offset < 0)
        offset = 0;

    RecvInto(self, args[0], (int) offset, 0);

    return nullptr;
}

ARGON_METHOD(socket_recv, recv,
             "Receive data from socket.\n"
             "\n"
             "- Parameters:\n"
             "  - size: Buffer size.\n"
             "  - flags: Flags.\n"
             "- Returns: Bytes object.\n",
             "i: size, i: flags", false, false) {
    auto *self = (Socket *) _self;

    IntegerUnderlying bufsize = ((Integer *) args[0])->sint;

    if (bufsize < 0)
        ErrorFormat(kValueError[0], "size cannot be less than zero");
    else
        Recv(self, bufsize, (int) ((Integer *) args[1])->sint);

    return nullptr;
}

ARGON_METHOD(socket_recvfrom, recvfrom,
             "Receive data from socket.\n"
             "\n"
             "- Parameters:\n"
             "  - size: Buffer size.\n"
             "  - flags: Flags.\n"
             "- Returns: Bytes object.\n",
             "i: size, i: flags", false, false) {
    auto *self = (Socket *) _self;

    IntegerUnderlying bufsize = ((Integer *) args[0])->sint;

    if (bufsize < 0)
        ErrorFormat(kValueError[0], "size cannot be less than zero");
    else
        RecvFrom(self, bufsize, (int) ((Integer *) args[1])->sint);

    return nullptr;
}

ARGON_METHOD(socket_send, send,
             "Send data to socket.\n"
             "\n"
             "- Parameters:\n"
             "  - buffer: Bytes-like object.\n"
             "  - nbytes: Maximum number of bytes to send, if omitted the value is equal to the length of the buffer.\n"
             "  - flags: Flags.\n"
             "- Returns: Bytes sent.\n",
             ": obj, i: nbytes, i: flags", false, false) {
    auto *self = (Socket *) _self;

    Send(self, *args, ((Integer *) args[2])->sint, (int) ((Integer *) args[2])->sint);

    return nullptr;
}

ARGON_METHOD(socket_sendto, sendto,
             "Send data to the socket.\n"
             "\n"
             "- Parameters:\n"
             "  - dest: Destination address.\n"
             "  - buffer: Bytes-like object.\n"
             "  - nbytes: Maximum number of bytes to send, if omitted the value is equal to the length of the buffer.\n"
             "  - flags: Flags.\n"
             "- Returns: Bytes sent.\n",
             " : dest, : obj, i: nbytes, i: flags", false, false) {
    auto *self = (Socket *) _self;

    SendTo(self, args[0], args[1], ((Integer *) args[2])->sint, (int) ((Integer *) args[3])->sint);

    return nullptr;
}

ARGON_METHOD(socket_setinheritable, setinherit,
             "Set the inheritable flag of the socket.\n"
             "\n"
             "- Parameters:\n"
             "  - inheritable: Set inheritable mode (true|false).\n",
             "b: inheritable", false, false) {
    return (ArObject *) SetInheritable(((Socket *) *args), ArBoolToBool((Boolean *) args[1]));
}

// Inherited from Writer trait
ARGON_METHOD_INHERITED(socket_write, write) {
    auto *self = (Socket *) _self;

    Send(self, *args, -1, 0);

    return nullptr;
}

const FunctionDef sock_methods[] = {
        socket_socket,

        socket_accept,
        socket_bind,
        socket_connect,
        socket_close,
        socket_detach,
        socket_dup,
        socket_listen,
        socket_read,
        socket_readinto,
        socket_recv,
        socket_recvfrom,
        socket_send,
        socket_sendto,
        socket_setinheritable,
        socket_write,
        ARGON_METHOD_SENTINEL
};

ArObject *sock_member_get_inheritable(const Socket *sock) {
    return BoolToArBool(IsInheritable(sock));
}

ArObject *sock_member_get_peername(const Socket *sock) {
    return PeerName(sock);
}

ArObject *sock_member_get_sockname(const Socket *sock) {
    return SockName(sock);
}

const MemberDef sock_members[] = {
        ARGON_MEMBER_GETSET("inheritable", (MemberGetFn) sock_member_get_inheritable, nullptr),
        ARGON_MEMBER_GETSET("peername", (MemberGetFn) sock_member_get_peername, nullptr),
        ARGON_MEMBER_GETSET("sockname", (MemberGetFn) sock_member_get_sockname, nullptr),
        ARGON_MEMBER_SENTINEL
};

TypeInfo *sock_bases[] = {
        (TypeInfo *) argon::vm::io::type_reader_t_,
        (TypeInfo *) argon::vm::io::type_writer_t_,
        nullptr
};

const ObjectSlots sock_objslot = {
        sock_methods,
        sock_members,
        sock_bases,
        nullptr,
        nullptr,
        -1
};

ArObject *socket_compare(const Socket *self, const ArObject *other, CompareMode mode) {
    auto *o = (const Socket *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != o)
        return BoolToArBool(self->sock == o->sock);

    return BoolToArBool(true);
}

ArObject *socket_repr(const Socket *self) {
    return (ArObject *) StringFormat("<socket fd: %d, family: %d, type: %d, protocol: %d>", self->sock,
                                     self->family, self->type, self->protocol);
}

bool socket_dtor(Socket *self) {
    if (self->sock != SOCK_HANDLE_INVALID) {
        Close(self);
    }

#ifndef _ARGON_PLATFORM_WINDOWS
    argon::vm::loop::EventQueueDel(&self->queue);
#endif

    return true;
}

bool socket_is_true(const Socket *self) {
    return self->sock != SOCK_HANDLE_INVALID;
}

TypeInfo SocketType = {
        AROBJ_HEAD_INIT_TYPE,
        "Socket",
        nullptr,
        nullptr,
        sizeof(Socket),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) socket_dtor,
        nullptr,
        nullptr,
        (Bool_UnaryOp) socket_is_true,
        (CompareOp) socket_compare,
        (UnaryConstOp) socket_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &sock_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::io::socket::type_socket_ = &SocketType;

ArObject *argon::vm::io::socket::SockAddrToAddr(sockaddr_storage *storage, int family) {
    char saddr[INET6_ADDRSTRLEN];

    switch (family) {
        case AF_INET: {
            auto addr = (sockaddr_in *) storage;
            inet_ntop(family, &addr->sin_addr, saddr, INET6_ADDRSTRLEN);
            return (ArObject *) TupleNew("sH", saddr, ntohs(addr->sin_port));
        }
        case AF_INET6: {
            auto addr = (sockaddr_in6 *) storage;
            inet_ntop(family, &addr->sin6_addr, saddr, INET6_ADDRSTRLEN);
            return (ArObject *) TupleNew("sHII", saddr, ntohs(addr->sin6_port), ntohs(addr->sin6_flowinfo),
                                         ntohs(addr->sin6_scope_id));
        }
#ifndef _ARGON_PLATFORM_WINDOWS
            case AF_UNIX:
                return (ArObject *) StringNew(((sockaddr_un *) storage)->sun_path);
#endif
        default:
            ErrorFormat(kOSError[0], "unsupported address family");
            return nullptr;
    }
}

bool argon::vm::io::socket::AddrToSockAddr(ArObject *addr, sockaddr_storage *store, socklen_t *len, int family) {
    char *saddr;

    *len = sizeof(sockaddr_in);
    store->ss_family = (unsigned char) family;

    switch (family) {
        case AF_INET: {
            auto *in = (sockaddr_in *) store;

            if (!TupleUnpack((Tuple *) addr, "sh", &saddr, &in->sin_port))
                return false;

            in->sin_port = htons(in->sin_port);

            if (inet_pton(family, saddr, &(in->sin_addr.s_addr)) > 0)
                return true;

            break;
        }
        case AF_INET6: {
            auto *in = (sockaddr_in6 *) store;

            if (!TupleUnpack((Tuple *) addr, "shII", &saddr, &in->sin6_port, &in->sin6_flowinfo, &in->sin6_scope_id))
                return false;

            in->sin6_port = htons(in->sin6_port);
            in->sin6_flowinfo = htonl(in->sin6_flowinfo);
            in->sin6_scope_id = htonl(in->sin6_scope_id);
            *len = sizeof(sockaddr_in6);

            if (inet_pton(family, saddr, &((sockaddr_in *) store)->sin_addr.s_addr) >= 0)
                return true;

            break;
        }
            // TODO: AF_UNIX
        default:
            ErrorFormat(kOSError[0], "unsupported address family");
            return false;
    }

    ErrorFormat(kValueError[0], "invalid network address");
    return false;
}

int argon::vm::io::socket::SocketAddrLen(const Socket *sock) {
    switch (sock->family) {
        case AF_INET:
            return sizeof(sockaddr_in);
        case AF_INET6:
            return sizeof(sockaddr_in6);
        default:
            ErrorFormat(kOSError[0], "SocketGetAddrLen: unknown protocol");
            return 0;
    }
}

void argon::vm::io::socket::ErrorFromSocket() {
#ifdef _ARGON_PLATFORM_WINDOWS
    auto *error = ErrorNewFromSocket();

    if (error != nullptr)
        argon::vm::Panic((ArObject *) error);

    Release(error);
#else
    ErrorFromErrno(errno);
#endif
}
