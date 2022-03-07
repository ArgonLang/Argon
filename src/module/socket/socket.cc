// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <WinSock2.h>
#include <WS2tcpip.h>

#undef CONST
#else

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/fcntl.h>

#endif

#include "socket.h"

#include <vm/runtime.h>

#include "object/datatype/bool.h"
#include "object/datatype/error.h"
#include "object/datatype/nil.h"
#include "object/datatype/integer.h"
#include "object/datatype/bytes.h"

using namespace argon::object;
using namespace argon::module::socket;

struct SockAdrr {
    sockaddr_storage storage;
    socklen_t socklen;
};

struct BufferObj {
    sockaddr_storage storage;
    char *buffer;
    ArSize buflen;
    ArSize namelen;
    int flags;
};

struct SockMsg {
#ifdef _ARGON_PLATFORM_WINDOWS
    WSAMSG msg;
#else
    msghdr msg;
#endif
    int flags;
};

using SockWrap = ArSSize (*)(Socket *socket, void *data);

ArObject *argon::module::socket::ErrorNewFromSocket() {
#ifdef _ARGON_PLATFORM_WINDOWS
    LPSTR estr;
    String *astr;
    ArObject *err;
    Tuple *etuple;

    int ecode = WSAGetLastError();
    int length;

    length = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                           FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, ecode, 0, (LPSTR) &estr, 0, nullptr);

    if (length == 0)
        return ErrorFormat(type_os_error_, "unable to obtain error message");

    // Remove space at the end of the error string
    if ((astr = StringNew(estr, length - 1)) == nullptr) {
        LocalFree(estr);
        return nullptr;
    }

    LocalFree(estr);

    if ((etuple = TupleNew("ia", ecode, astr)) == nullptr) {
        Release(astr);
        return nullptr;
    }

    Release(astr);

    if ((err = ErrorNew(type_wsa_error_, etuple)) == nullptr) {
        Release(etuple);
        return nullptr;
    }

    Release(etuple);

    return err;
#else
    return ErrorNewFromErrno();
#endif
}

inline ArObject *argon::module::socket::ErrorSetFromSocket() {
#ifdef _ARGON_PLATFORM_WINDOWS
    ArObject *err = ErrorNewFromSocket();

    if (err != nullptr) {
        argon::vm::Panic(err);
        Release(err);
    }

    return nullptr;
#else
    return ErrorSetFromErrno();
#endif
}

ArSSize SockAccept(Socket *socket, void *data) {
    auto *addr = (SockAdrr *) data;

    return accept(socket->sock, (sockaddr *) &addr->storage, (socklen_t *) &addr->socklen);
}

ArSSize SockBind(Socket *socket, void *data) {
    auto *addr = (SockAdrr *) data;

    return bind(socket->sock, (sockaddr *) &addr->storage, addr->socklen);
}

ArSSize SockConnect(Socket *socket, void *data) {
    auto *addr = (SockAdrr *) data;

    return connect(socket->sock, (sockaddr *) &addr->storage, addr->socklen);
}

ArSSize SockRecv(Socket *socket, void *data) {
    auto *buf = (BufferObj *) data;

    buf->namelen = sizeof(sockaddr_storage);

    return recvfrom(socket->sock, buf->buffer, buf->buflen, buf->flags, (sockaddr *) &buf->storage,
                    (socklen_t *) &buf->namelen);
}

#ifndef _ARGON_PLATFORM_WINDOWS

ArSSize SockRecvMsg(Socket *socket, void *data) {
    auto *msg = (SockMsg *) data;

    return recvmsg(socket->sock, &msg->msg, msg->flags);
}

ArSSize SockSendMsg(Socket *socket, void *data) {
    auto *msg = (SockMsg *) data;

    return sendmsg(socket->sock, &msg->msg, msg->flags);
}

#endif

ArSSize SockSend(Socket *socket, void *data) {
    auto *buf = (BufferObj *) data;

    return sendto(socket->sock, buf->buffer, buf->buflen, buf->flags, (sockaddr *) &buf->storage,
                  sizeof(sockaddr_storage));
}

ArSSize SockCall(Socket *socket, SockWrap func, void *data, ArObject **out_err) {
    ArSSize err;

    *out_err = nullptr;

    do {
        if ((err = func(socket, data)) < 0) {
#ifdef _ARGON_PLATFORM_WINDOWS
            if (WSAGetLastError() != WSAEINTR) {
                *out_err = ErrorNewFromSocket();
                break;
            }
#else
            if (errno != EINTR) {
                *out_err = ErrorNewFromSocket();
                break;
            }
#endif
        }
    } while (err < 0);

    return err;
}

ARGON_METHOD5(socket_, accept, "Accept a connection."
                               ""
                               "The socket must be bound to an address and listening for connections."
                               ""
                               "- Returns: (socket, err)", 0, false) {
    SockAdrr addr{};
    auto *socket = (Socket *) self;
    ArObject *err;
    Socket *ret;
    sock_handle handle;

    if ((handle = (sock_handle) SockCall(socket, SockAccept, &addr, &err)) < 0)
        return ARGON_OBJECT_TUPLE_ERROR(err);

    if ((ret = SocketNew(handle, socket->family)) == nullptr) {
        CloseHandle(handle);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    return ARGON_OBJECT_TUPLE_SUCCESS(ret);
}

ARGON_METHOD5(socket_, bind, "Bind the socket to address."
                             ""
                             "The socket must not already be bound."
                             ""
                             "- Parameter address: format of address depends on the address family."
                             "- Returns: nil|err", 1, false) {
    SockAdrr addr{};
    auto *socket = (Socket *) self;
    ArObject *err;

    if (!CheckArgs("ts:address", func, argv, count, type_string_))
        return nullptr;

    if (!ArAddrToSockAddr(argv[0], &addr.storage, (int *) &addr.socklen, socket->family))
        return argon::vm::GetLastNonFatalError();

    if (SockCall(socket, SockBind, &addr, &err) != 0)
        return err;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(socket_, connect, "Connect to a remote socket at address."
                                ""
                                "- Parameter address: format of address depends on the address family."
                                "- Returns: nil|err", 1, false) {
    SockAdrr addr{};
    auto *socket = (Socket *) self;
    ArObject *err;

    if (!CheckArgs("ts:address", func, argv, count, type_string_))
        return nullptr;

    if (!ArAddrToSockAddr(argv[0], &addr.storage, (int *) &addr.socklen, socket->family))
        return argon::vm::GetLastNonFatalError();

    if (SockCall(socket, SockConnect, &addr, &err) != 0)
        return err;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(socket_, close, "Mark the socket closed."
                              ""
                              "The underlying file descriptor is also closed."
                              ""
                              "- Returns: nil", 0, false) {
    Close((Socket *) self);
    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(socket_, detach, "Put the socket into closed state."
                               ""
                               "This method does not affect the underlying file descriptor."
                               ""
                               "- Returns: file descriptor.", 0, false) {
    auto *socket = (Socket *) self;
    ArObject *obj;

    if ((obj = IntegerNew(socket->sock)) == nullptr)
        return nullptr;

    socket->sock = SOCK_HANDLE_INVALID;
    return obj;
}


ARGON_METHOD5(socket_, dup, "Duplicate the socket."
                            ""
                            "- Returns: (duplicate socket, err)", 0, false) {
#ifdef _ARGON_PLATFORM_WINDOWS
    _WSAPROTOCOL_INFOW info{};
#endif
    auto *socket = (Socket *) self;
    ArObject *err = nullptr;
    Socket *ret;
    sock_handle handle;

#ifdef _ARGON_PLATFORM_WINDOWS
    if (WSADuplicateSocketW(socket->sock, GetCurrentProcessId(), &info) != 0)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());

    handle = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                        &info, 0, WSA_FLAG_NO_HANDLE_INHERIT);

    if (handle == INVALID_SOCKET)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromSocket());

    if ((ret = SocketNew(handle, socket->family)) == nullptr) {
        CloseHandle(handle);

        if ((err = argon::vm::GetLastNonFatalError()) == nullptr)
            return nullptr;

        return ARGON_OBJECT_TUPLE_ERROR(err);
    }
#else
    if ((handle = dup(socket->sock)) < 0)
        return ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromErrno());

    if ((ret = SocketNew(handle, socket->family)) == nullptr) {
        CloseHandle(handle);

        if ((err = argon::vm::GetLastNonFatalError()) == nullptr)
            return nullptr;
    }
#endif

    return TupleReturn(ret, err);
}


ARGON_METHOD5(socket_, inheritable, "Get the inheritable flag of the socket."
                                    ""
                                    "- Returns: true if the socket can be inherited in child processes, false otherwise.",
              0, false) {
#ifdef _ARGON_PLATFORM_WINDOWS
    unsigned long flags;
    int err;

    if ((err = GetHandleInformation((HANDLE) ((Socket *) self)->sock, &flags)) == 0) {
        // TODO ERROR
    }

    return BoolToArBool((flags & HANDLE_FLAG_INHERIT) == HANDLE_FLAG_INHERIT);
#else
    int flags = SocketGetFlags((Socket *) self, F_GETFD);
    return BoolToArBool((flags & FD_CLOEXEC) != FD_CLOEXEC);
#endif
}


ARGON_METHOD5(socket_, setinheritable, "Set the inheritable flag of the socket."
                                       ""
                                       "- Parameter inheritable: bool"
                                       "- Returns: nil|err", 1, false) {
    if (!CheckArgs("b:inheritable", func, argv, count, type_string_))
        return nullptr;

    if (!SocketSetInheritable((Socket *) self, ((Bool *) argv[0])->value))
        return ErrorNewFromSocket();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(socket_, peername, "Return the remote address to which the socket is connected."
                                 ""
                                 "- Returns: address", 0, false) {
    sockaddr_storage storage{};
    auto *socket = (Socket *) self;
    int namelen;

    if ((namelen = SocketGetAddrLen(socket)) == 0)
        return nullptr;

    if (getpeername(socket->sock, (sockaddr *) &storage, (socklen_t *) &namelen) != 0)
        return ErrorSetFromSocket();

    return SockAddrToArAddr(&storage, socket->family);
}

ARGON_METHOD5(socket_, sockname, "Return the socket’s own address."
                                 ""
                                 "- Returns: address", 0, false) {
    sockaddr_storage storage{};
    auto *socket = (Socket *) self;
    int namelen;

    if ((namelen = SocketGetAddrLen(socket)) == 0)
        return nullptr;

    if (getsockname(socket->sock, (sockaddr *) &storage, (socklen_t *) &namelen) != 0)
        return ErrorSetFromSocket();

    return SockAddrToArAddr(&storage, socket->family);
}

#ifndef _ARGON_PLATFORM_WINDOWS

ARGON_METHOD5(socket_, blocking, "Get blocking flag of the socket."
                                 ""
                                 "- Returns: true if the socket is in blocking mode, false otherwise.", 0, false) {
    int flags = SocketGetFlags((Socket *) self, F_GETFD);
    return BoolToArBool((flags & O_NONBLOCK) != O_NONBLOCK);
}

#endif

ARGON_METHOD5(socket_, setblocking, "Set blocking flag of the socket."
                                    ""
                                    "- Returns: nil|err", 1, false) {
    if (!CheckArgs("b:blocking", func, argv, count, type_string_))
        return nullptr;

    if (!SocketSetNonBlock((Socket *) self, !((Bool *) argv[0])->value))
        return ErrorNewFromSocket();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(socket_, listen, "Enable a server to accept connections."
                               ""
                               "If backlog is specified, it must be at least 0. It specifies the number of unaccepted "
                               "connections that the system will allow before refusing new connections."
                               ""
                               "- Returns: nil|err", 1, false) {
    auto *socket = (Socket *) self;
    IntegerUnderlying backlog;

    if (!CheckArgs("i:backlog", func, argv, count))
        return nullptr;

    backlog = ((Integer *) argv[0])->integer;

    if (backlog < 0 || backlog > SOMAXCONN)
        backlog = SOMAXCONN;

    if (listen(socket->sock, (int) backlog) != 0)
        return ErrorNewFromSocket();

    return ARGON_OBJECT_NIL;
}

ArSSize RecvWrapper(Socket *socket, BufferObj *buffer, ArObject **out_err) {
    bool allocated = false;
    ArSSize recvlen;

    *out_err = nullptr;

    if (buffer->buffer == nullptr) {
        allocated = true;

        if ((buffer->buffer = ArObjectNewRaw<char *>(buffer->buflen)) == nullptr)
            return -1;
    }

    if ((recvlen = SockCall(socket, SockRecv, buffer, out_err)) < 0) {
        if (allocated)
            argon::memory::Free(buffer->buffer);
        return -1;
    }

    return recvlen;
}

ARGON_METHOD5(socket_, recv, "Receive data from the socket."
                             ""
                             "- Parameters:"
                             "  - bufsize: buffer size."
                             "  - flags: flags."
                             "- Returns: (bytes, err)", 2, false) {
    BufferObj buffer{};
    auto *socket = (Socket *) self;
    ArObject *err;
    Bytes *bytes;
    ArSSize recvlen;

    if (!CheckArgs("i:bufsize,i:flags", func, argv, count))
        return nullptr;

    buffer.buflen = ((Integer *) argv[0])->integer;
    buffer.flags = (int) ((Integer *) argv[1])->integer;

    if ((recvlen = RecvWrapper(socket, &buffer, &err)) < 0)
        return ARGON_OBJECT_TUPLE_ERROR(err);

    if ((bytes = BytesNewHoldBuffer((unsigned char *) buffer.buffer, recvlen, buffer.buflen, true)) == nullptr) {
        argon::memory::Free(buffer.buffer);
        return nullptr;
    }

    return ARGON_OBJECT_TUPLE_SUCCESS(bytes);
}

ARGON_METHOD5(socket_, recv_into, "Receive data from the socket and writing it into buffer."
                                  ""
                                  "- Parameters:"
                                  "  - buffer: writable bytes-like object."
                                  "  - flags: flags."
                                  "- Returns: (bytes read, err)", 2, false) {
    BufferObj buffer{};
    ArBuffer view{};
    auto *socket = (Socket *) self;
    ArObject *err;
    Integer *nbytes;
    ArSSize recvlen;

    if (!CheckArgs("B:buffer,i:flags", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &view, ArBufferFlags::WRITE))
        return nullptr;

    buffer.buffer = (char *) view.buffer;
    buffer.buflen = view.len;

    if ((recvlen = RecvWrapper(socket, &buffer, &err)) < 0) {
        if (err == nullptr) {
            BufferRelease(&view);
            return nullptr;
        }
    }

    BufferRelease(&view);

    if ((nbytes = IntegerNew(recvlen)) == nullptr)
        return nullptr;

    return TupleReturn(nbytes, err);
}

ARGON_METHOD5(socket_, recvfrom, "Receive data from the socket."
                                 ""
                                 "- Parameters:"
                                 "  - bufsize: buffer size."
                                 "  - flags: flags."
                                 "- Returns: (bytes, address, err)", 2, false) {
    auto *socket = (Socket *) self;
    BufferObj buffer{};
    ArObject *addr;
    ArObject *err;
    Bytes *bytes;
    Tuple *ret;
    ArSSize recvlen;

    if (!CheckArgs("i:bufsize,i:flags", func, argv, count))
        return nullptr;

    buffer.buflen = ((Integer *) argv[0])->integer;
    buffer.flags = (int) ((Integer *) argv[1])->integer;

    if ((recvlen = RecvWrapper(socket, &buffer, &err)) < 0) {
        if (err == nullptr)
            return nullptr;

        ret = TupleNew("aaa", NilVal, NilVal, err);
        Release(err);

        return ret;
    }

    if ((bytes = BytesNewHoldBuffer((unsigned char *) buffer.buffer, recvlen, buffer.buflen, true)) == nullptr) {
        argon::memory::Free(buffer.buffer);
        return nullptr;
    }

    if ((addr = SockAddrToArAddr(&buffer.storage, socket->family)) == nullptr) {
        Release(bytes);
        return nullptr;
    }

    ret = TupleNew("aaa", bytes, addr, NilVal);
    Release(bytes);
    Release(addr);

    return ret;
}

ARGON_METHOD5(socket_, recvfrom_into, "Receive data from the socket."
                                      ""
                                      "- Parameters:"
                                      "  - buffer: writable bytes-like object."
                                      "  - flags: flags."
                                      "- Returns: (bytes read, address, err)", 2, false) {
    ArBuffer buffer{};
    BufferObj sockbuf{};
    auto *socket = (Socket *) self;
    ArObject *addr = nullptr;
    ArObject *err;
    Tuple *ret;
    ArSSize recvlen;

    if (!CheckArgs("B:buffer,i:flags", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::WRITE))
        return nullptr;

    sockbuf.buffer = (char *) buffer.buffer;
    sockbuf.buflen = buffer.len;
    sockbuf.flags = (int) ((Integer *) argv[1])->integer;

    if ((recvlen = RecvWrapper(socket, &sockbuf, &err)) < 0) {
        if (err == nullptr) {
            BufferRelease(&buffer);
            return nullptr;
        }
    }

    BufferRelease(&buffer);

    if (recvlen >= 0 && (addr = SockAddrToArAddr(&sockbuf.storage, socket->family)) == nullptr)
        return nullptr;

    ret = TupleNew("iaa", recvlen, addr, err);
    Release(addr);
    Release(err);

    return ret;
}

#ifndef _ARGON_PLATFORM_WINDOWS

Tuple *ParseAncillary(cmsghdr *cmsg) {
    ArSize datalen = cmsg->cmsg_len - sizeof(cmsghdr);
    unsigned char *data;
    Bytes *bytes;
    Tuple *ret;

    if ((data = ArObjectNewRaw<unsigned char *>(datalen)) == nullptr)
        return nullptr;

    argon::memory::MemoryCopy(data, CMSG_DATA(cmsg), datalen);

    if ((bytes = BytesNewHoldBuffer(data, datalen, datalen, true)) == nullptr) {
        argon::memory::Free(data);
        return nullptr;
    }

    ret = TupleNew("iia", cmsg->cmsg_level, cmsg->cmsg_type, bytes);
    Release(bytes);

    return ret;
}

List *ParseMsgHdr(msghdr *msgs) {
    cmsghdr *cmsg = nullptr;
    Tuple *tmp;
    List *ret;

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    if (msgs->msg_controllen > 0)
        cmsg = CMSG_FIRSTHDR(msgs);

    while (cmsg != nullptr) {
        if ((tmp = ParseAncillary(cmsg)) == nullptr) {
            Release(ret);
            goto ERROR;
        }

        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(ret);
            goto ERROR;
        }

        Release(tmp);
        cmsg = CMSG_NXTHDR(msgs, cmsg);
    }

    return ret;

    ERROR:

#ifdef SCM_RIGHTS
    // Close all descriptors

    if (msgs->msg_controllen > 0)
        cmsg = CMSG_FIRSTHDR(msgs);

    while (cmsg != nullptr) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            ArSize fdcount = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *fdptr = (int *) CMSG_DATA(cmsg);

            while (fdcount-- > 0)
                close(*fdptr++);
        }

        cmsg = CMSG_NXTHDR(msgs, cmsg);
    }
#endif

    return nullptr;
}

bool PrepareRecvMsg(SockMsg *msg) {
    iovec *iov = msg->msg.msg_iov;
    bool aiov = false;

    if (iov->iov_len < 0)
        iov->iov_len = 0;

    if (msg->msg.msg_controllen < 0)
        msg->msg.msg_controllen = 0;

    if (iov->iov_base == nullptr && iov->iov_len > 0) {
        aiov = true;

        if ((iov->iov_base = ArObjectNewRaw<char *>(iov->iov_len)) == nullptr)
            return false;
    }

    if (msg->msg.msg_control == nullptr && msg->msg.msg_controllen > 0) {
        if ((msg->msg.msg_control = ArObjectNewRaw<char *>(msg->msg.msg_controllen)) == nullptr) {
            if (aiov)
                argon::memory::Free(iov->iov_base);

            return false;
        }
    }

    return true;
}

bool PrepareSendMsg(ArObject *iterable, msghdr *msg) {
    ArBuffer buffer{};
    cmsghdr *cmsg = nullptr;
    ArObject *bytes = nullptr;
    ArObject *iter;
    Tuple *tuple;
    char *tmp;

    int level;
    int type;
    ArSize buflen = 0;

    if ((iter = IteratorGet(iterable)) == nullptr)
        return false;

    while ((tuple = (Tuple *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tuple, type_tuple_)) {
            ErrorFormat(type_type_error_, "expected tuple as ancillary data, found: '%s'", AR_TYPE_NAME(tuple));
            goto ERROR;
        }

        if (!TupleUnpack(tuple, "iia", &level, &type, &bytes))
            goto ERROR;

        if (!BufferGet(bytes, &buffer, ArBufferFlags::READ))
            goto ERROR;

        while (msg->msg_control == nullptr || (sizeof(cmsghdr) + buffer.len) >= buflen - msg->msg_controllen) {
            buflen += 1024;

            if ((tmp = ArObjectRealloc<char *>((char *) msg->msg_control, buflen)) == nullptr) {
                BufferRelease(&buffer);
                goto ERROR;
            }

            if (msg->msg_control)
                cmsg = CMSG_FIRSTHDR(msg);

            msg->msg_control = tmp;
        }

        msg->msg_controllen += sizeof(cmsghdr);

        cmsg = CMSG_NXTHDR(msg, cmsg);

        argon::memory::MemoryCopy(CMSG_DATA(cmsg), buffer.buffer, buffer.len);

        cmsg->cmsg_level = level;
        cmsg->cmsg_type = type;
        cmsg->cmsg_len = buffer.len;

        BufferRelease(&buffer);
        Release((ArObject **) &bytes);
        Release((ArObject **) &tuple);

        msg->msg_controllen += buffer.len;
    }

    Release(iter);

    return true;

    ERROR:
    Release(bytes);
    Release(tuple);
    Release(iter);
    argon::memory::Free(msg->msg_control);
    return false;
}

ARGON_METHOD5(socket_, recvmsg, "Receive data from the socket and writing it into buffer."
                                ""
                                "- Parameters:"
                                "  - buffer: maximum number of bytes to write into buffer."
                                "  - ancsize: maximum number of bytes to write into ancillary buffer."
                                "  - flags: flags."
                                "- Returns: (bytes, ancillary, address, err)", 3, false) {
    sockaddr_storage storage{};
    SockMsg msg{};
    iovec iov{};
    auto *socket = (Socket *) self;
    ArObject *addr = nullptr;
    List *ancillary = nullptr;
    Tuple *ret = nullptr;
    Bytes *data = nullptr;
    ArObject *err;

    ArSSize buflen;

    if (!CheckArgs("i:bufsize,i:ancsize,i:flags", func, argv, count))
        return nullptr;

    msg.msg.msg_name = &storage;
    msg.msg.msg_namelen = sizeof(sockaddr_storage);

    iov.iov_len = ((Integer *) argv[0])->integer;

    msg.msg.msg_iov = &iov;
    msg.msg.msg_iovlen = 1;

    msg.msg.msg_controllen = ((Integer *) argv[1])->integer;

    if (!PrepareRecvMsg(&msg))
        goto ERROR;

    if ((buflen = SockCall(socket, SockRecvMsg, &msg, &err)) < 0)
        goto ERROR;

    if ((data = BytesNewHoldBuffer((unsigned char *) iov.iov_base, buflen, iov.iov_len, true)) == nullptr)
        goto ERROR;

    if ((ancillary = ParseMsgHdr(&msg.msg)) == nullptr)
        goto ERROR;

    if ((addr = SockAddrToArAddr(&storage, socket->family)) == nullptr)
        goto ERROR;

    ret = TupleNew("aaaa", data, ancillary, addr, err);

    ERROR:
    Release(data);
    if (data == nullptr)
        argon::memory::Free(iov.iov_base);
    Release(ancillary);
    Release(addr);
    argon::memory::Free(msg.msg.msg_control);

    return ret == nullptr ? nullptr : ret;
}

ARGON_METHOD5(socket_, recvmsg_into, "Receive data from the socket and writing it into buffer."
                                     ""
                                     "- Parameters:"
                                     "  - buffer: writable bytes-like object."
                                     "  - ancsize: maximum number of bytes to write into ancillary buffer."
                                     "  - flags: flags."
                                     "- Returns: (bytes read, ancillary, address, err)", 3, false) {
    sockaddr_storage storage{};
    ArBuffer buffer{};
    SockMsg msg{};
    iovec iov{};
    auto *socket = (Socket *) self;
    ArObject *addr = nullptr;
    List *ancillary = nullptr;
    Tuple *ret = nullptr;
    ArObject *err;

    ArSSize buflen;

    if (!CheckArgs("B:buffer,i:ancsize,i:flags", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::WRITE))
        return nullptr;

    msg.msg.msg_name = &storage;
    msg.msg.msg_namelen = sizeof(sockaddr_storage);

    iov.iov_base = buffer.buffer;
    iov.iov_len = buffer.len;

    msg.msg.msg_iov = &iov;
    msg.msg.msg_iovlen = 1;

    msg.msg.msg_controllen = ((Integer *) argv[1])->integer;

    if (!PrepareRecvMsg(&msg))
        goto ERROR;

    if ((buflen = SockCall(socket, SockRecvMsg, &msg, &err)) < 0)
        goto ERROR;

    if ((ancillary = ParseMsgHdr(&msg.msg)) == nullptr)
        goto ERROR;

    if ((addr = SockAddrToArAddr(&storage, socket->family)) == nullptr)
        goto ERROR;

    ret = TupleNew("iaaa", buflen, ancillary, addr, err);

    ERROR:
    BufferRelease(&buffer);
    Release(ancillary);
    Release(addr);
    argon::memory::Free(msg.msg.msg_control);

    return ret == nullptr ? nullptr : ret;
}

#endif

ArSize GetBufferSize(ArSize realsize, Integer *desired) {
    if (IsNull(desired))
        return realsize;

    if (desired->integer < 0)
        return realsize;

    return desired->integer < realsize ? desired->integer : realsize;
}

ARGON_METHOD5(socket_, send, "Send data to the socket."
                             ""
                             "- Parameters:"
                             "  - buffer: bytes-like object."
                             "  - nbytes: maximum number of bytes to send, if omitted the value is equal to the length of the buffer."
                             "  - flags: flags."
                             "- Returns: (bytes sent, err)", 3, false) {
    ArBuffer buffer{};
    BufferObj sockbuf{};
    auto *socket = (Socket *) self;
    ArObject *err;
    Integer *len;
    ArSSize nbytes;

    if (!CheckArgs("B:buffer,i?:nbytes,i:flags", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    sockbuf.buffer = (char *) buffer.buffer;
    sockbuf.buflen = GetBufferSize(buffer.len, (Integer *) argv[1]);
    sockbuf.flags = (int) ((Integer *) argv[2])->integer;

    if ((nbytes = SockCall(socket, SockSend, &sockbuf, &err)) < 0) {
        if (err == nullptr) {
            BufferRelease(&buffer);
            return nullptr;
        }
    }

    BufferRelease(&buffer);

    if ((len = IntegerNew(nbytes)) == nullptr)
        return nullptr;

    return TupleReturn(len, err);
}

ARGON_METHOD5(socket_, sendto, "Send data to the socket."
                               ""
                               "- Parameters:"
                               "  - buffer: bytes-like object."
                               "  - nbytes: maximum number of bytes to send, if omitted the value is equal to the length of the buffer."
                               "  - flags: flags."
                               "  - address: format of address depends on the address family."
                               "- Returns: (bytes sent, err)", 4, false) {
    ArBuffer buffer{};
    BufferObj sockbuf{};
    auto *socket = (Socket *) self;
    ArObject *err;
    Integer *len;
    ArSSize nbytes;

    if (!CheckArgs("B:buffer,i?:nbytes,i:flags,ts:address", func, argv, count))
        return nullptr;

    if (!ArAddrToSockAddr(argv[3], &sockbuf.storage, (int *) &sockbuf.namelen, socket->family)) {
        if ((err = argon::vm::GetLastNonFatalError()) == nullptr)
            return nullptr;

        if ((len = IntegerNew(-1)) == nullptr) {
            Release(err);
            return nullptr;
        }

        return TupleReturn(len, ErrorNewFromSocket());
    }

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    sockbuf.buffer = (char *) buffer.buffer;
    sockbuf.buflen = GetBufferSize(buffer.len, (Integer *) argv[1]);
    sockbuf.flags = (int) ((Integer *) argv[2])->integer;

    if ((nbytes = SockCall(socket, SockSend, &sockbuf, &err)) < 0) {
        if (err == nullptr) {
            BufferRelease(&buffer);
            return nullptr;
        }
    }

    BufferRelease(&buffer);

    if ((len = IntegerNew(nbytes)) == nullptr)
        return nullptr;

    return TupleReturn(len, err);
}

#ifndef _ARGON_PLATFORM_WINDOWS

ARGON_METHOD5(socket_, sendmsg, "Send data to the socket."
                                ""
                                "- Parameters:"
                                "  - buffer: bytes-like object."
                                "  - ancdata: iterable of zero or more tuples(cmsg_level, cmsg_type, cmsg_Data)."
                                "  - flags: flags."
                                "  - address: format of address depends on the address family."
                                "- Returns: (bytes sent, err)", 4, false) {
    sockaddr_storage storage{};
    SockMsg msg{};
    ArBuffer data{};
    iovec iov{};
    auto socket = (Socket *) self;
    ArObject *err;
    Integer *len;

    ArSSize nbytes;

    if (!CheckArgs("B:buffer,I?:ancdata,i:flags,ts?:address", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &data, ArBufferFlags::READ))
        return nullptr;

    if (!IsNull(argv[1])) {
        if (!PrepareSendMsg(argv[1], &msg.msg)) {
            BufferRelease(&data);
            return nullptr;
        }
    }

    if (!IsNull(argv[3])) {
        if (!ArAddrToSockAddr(argv[3], &storage, (int *) &msg.msg.msg_namelen, socket->family)) {
            if ((err = argon::vm::GetLastNonFatalError()) == nullptr)
                return nullptr;

            if ((len = IntegerNew(-1)) == nullptr) {
                Release(err);
                return nullptr;
            }

            return TupleReturn(len, err);
        }

        msg.msg.msg_name = &storage;
    }

    iov.iov_base = data.buffer;
    iov.iov_len = data.len;

    msg.msg.msg_iov = &iov;
    msg.msg.msg_iovlen = 1;

    msg.flags = (int) ((Integer *) argv[2])->integer;

    nbytes = SockCall(socket, SockSendMsg, &msg, &err);

    BufferRelease(&data);
    argon::memory::Free(msg.msg.msg_control);

    if ((len = IntegerNew(nbytes)) == nullptr)
        return nullptr;

    return TupleReturn(len, err);
}

#endif

NativeFunc socket_method[] = {
        socket_accept_,
        socket_bind_,
#ifndef _ARGON_PLATFORM_WINDOWS
        socket_blocking_,
#endif
        socket_close_,
        socket_connect_,
        socket_detach_,
        socket_dup_,
        socket_inheritable_,
        socket_listen_,
        socket_peername_,
        socket_recv_,
        socket_recv_into_,
        socket_recvfrom_,
        socket_recvfrom_into_,
#ifndef _ARGON_PLATFORM_WINDOWS
        socket_recvmsg_,
        socket_recvmsg_into_,
        socket_sendmsg_,
#endif
        socket_send_,
        socket_sendto_,
        socket_setblocking_,
        socket_setinheritable_,
        socket_sockname_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots socket_obj = {
        socket_method,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *socket_compare(Socket *self, ArObject *other, CompareMode mode) {
    auto *o = (Socket *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other)
        return BoolToArBool(self->sock == o->sock);

    return BoolToArBool(true);
}

ArObject *socket_str(Socket *self) {
    return StringNewFormat("<socket fd: %d, family: %d>", self->sock, self->family);
}

bool socket_istrue(Socket *self) {
    return self->sock >= 0;
}

void socket_cleanup(Socket *self) {
    Close(self);
}

const TypeInfo SocketType = {
        TYPEINFO_STATIC_INIT,
        "socket",
        nullptr,
        sizeof(Socket),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) socket_cleanup,
        nullptr,
        (CompareOp) socket_compare,
        (BoolUnaryOp) socket_istrue,
        nullptr,
        (UnaryOp) socket_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &socket_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::socket::type_socket_ = &SocketType;

bool argon::module::socket::ArAddrToSockAddr(ArObject *araddr, sockaddr_storage *addrstore, int *socklen, int family) {
    char *saddr;

    *socklen = sizeof(sockaddr_in);
    addrstore->ss_family = family;

    switch (family) {
        case AF_INET: {
            auto *addr = (sockaddr_in *) addrstore;

            if (!TupleUnpack((Tuple *) araddr, "sH", &saddr, &addr->sin_port))
                return false;

            addr->sin_port = htons(addr->sin_port);

            if (inet_pton(family, saddr, &((sockaddr_in *) addrstore)->sin_addr.s_addr) >= 0)
                return true;

            break;
        }
        case AF_INET6: {
            auto *addr = (sockaddr_in6 *) addrstore;

            if (!TupleUnpack((Tuple *) araddr, "sHII", &saddr, &addr->sin6_port, &addr->sin6_flowinfo,
                             &addr->sin6_scope_id))
                return false;

            addr->sin6_port = htons(addr->sin6_port);
            addr->sin6_flowinfo = htons(addr->sin6_flowinfo);
            addr->sin6_scope_id = htons(addr->sin6_scope_id);
            *socklen = sizeof(sockaddr_in6);

            if (inet_pton(family, saddr, &((sockaddr_in *) addrstore)->sin_addr.s_addr) >= 0)
                return true;

            break;
        }
#if defined(_ARGON_PLATFORM_LINUX)
        case AF_UNIX: {
            auto *addr = (sockaddr_un *) addrstore;
            auto *str = (String *) araddr;

            argon::memory::MemoryCopy(addr->sun_path, str->buffer,
                                      str->len + 1 >= 104 ? 104 : str->len + 1);  // +1 -> '\0'
            return true;
        }
#elif defined(_ARGON_PLATFORM_DARWIN)
            case AF_UNIX: {
                auto *addr = (sockaddr_un *) addrstore;
                auto *str = (String *) araddr;
                addr->sun_len = 104;

                if (str->len + 1 < addr->sun_len)
                    addr->sun_len = str->len + 1;

                argon::memory::MemoryCopy(addr->sun_path, str->buffer, addr->sun_len);
                return true;
            }
#endif
        default:
            ErrorFormat(type_os_error_, "unsupported address family");
            return false;
    }

    return false;
}

int argon::module::socket::SocketGetAddrLen(Socket *socket) {
    switch (socket->family) {
        case AF_INET:
            return sizeof(sockaddr_in);
        case AF_INET6:
            return sizeof(sockaddr_in6);
        default:
            ErrorFormat(type_os_error_, "SocketGetAddrLen: unknown protocol");
            return 0;
    }
}

int argon::module::socket::SocketGetFlags(Socket *socket, int type) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return 0;
#else
    return fcntl(socket->sock, type, 0);
#endif
}

bool argon::module::socket::SocketSetFlags(Socket *socket, int type, long flags) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return ioctlsocket(socket->sock, type, (unsigned long *) &flags) == 0;
#else
    return fcntl(socket->sock, type, (int) flags) == 0;
#endif
}

bool argon::module::socket::SocketSetNonBlock(Socket *socket, bool blocking) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return SocketSetFlags(socket, FIONBIO, blocking);
#else
    int flags = SocketGetFlags(socket, F_GETFL);
    flags = blocking ? flags | O_NONBLOCK : flags & (~O_NONBLOCK);
    return SocketSetFlags(socket, F_SETFL, flags);
#endif
}

bool argon::module::socket::SocketSetInheritable(Socket *socket, bool inheritable) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return SetHandleInformation((HANDLE) socket->sock, HANDLE_FLAG_INHERIT, inheritable ? HANDLE_FLAG_INHERIT : 0);
#else
    int flags = SocketGetFlags(socket, F_GETFD);
    flags = !inheritable ? flags | FD_CLOEXEC : flags & (~FD_CLOEXEC);
    return SocketSetFlags(socket, F_SETFD, flags);
#endif
}

int argon::module::socket::CloseHandle(sock_handle handle) {
    int times = 3;
    int err;

#ifdef _ARGON_PLATFORM_WINDOWS
    do {
        err = 0;
        if (closesocket(handle) != 0)
            err = WSAGetLastError();
    } while ((err == WSAEINPROGRESS || err == WSAEINTR) && times-- > 0);
#else
    do {
        err = close(handle);
    } while (err != 0 && errno == EINTR && times-- > 0);
#endif

    return err;
}

Socket *argon::module::socket::SocketNew(int domain, int type, int protocol) {
    Socket *sock;
    sock_handle handle;

#ifdef _ARGON_PLATFORM_WINDOWS
    if ((handle = WSASocketW(domain, type, protocol, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT)) == INVALID_SOCKET)
        return (Socket *) ErrorSetFromSocket();
#else
    if ((handle = ::socket(domain, type, protocol)) < 0)
        return (Socket *) ErrorSetFromErrno();
#endif

    if ((sock = SocketNew(handle, domain)) == nullptr) {
        CloseHandle(handle);
        return (Socket *) ErrorSetFromSocket();
    }

    return sock;
}

Socket *argon::module::socket::SocketNew(sock_handle handle, int family) {
    Socket *sock;

    if ((sock = ArObjectNew<Socket>(RCType::INLINE, type_socket_)) == nullptr)
        return nullptr;

    sock->sock = handle;
    sock->family = family;

    if (!SocketSetInheritable(sock, false)) {
        sock->sock = SOCK_HANDLE_INVALID;
        Release(sock);
    }

    return sock;
}

ArObject *argon::module::socket::SockAddrToArAddr(sockaddr_storage *storage, int family) {
    char saddr[INET6_ADDRSTRLEN];

    switch (family) {
        case AF_INET: {
            auto addr = (sockaddr_in *) storage;
            inet_ntop(family, &addr->sin_addr, saddr, INET6_ADDRSTRLEN);
            return TupleNew("sH", saddr, ntohs(addr->sin_port));
        }
        case AF_INET6: {
            auto addr = (sockaddr_in6 *) storage;
            inet_ntop(family, &addr->sin6_addr, saddr, INET6_ADDRSTRLEN);
            return TupleNew("sHII", saddr, ntohs(addr->sin6_port), ntohs(addr->sin6_flowinfo),
                            ntohs(addr->sin6_scope_id));
        }
#ifndef _ARGON_PLATFORM_WINDOWS
        case AF_UNIX:
            return StringNew(((sockaddr_un *) storage)->sun_path);
#endif
        default:
            return (Tuple *) ErrorFormat(type_os_error_, "unsupported address family");
    }
}
