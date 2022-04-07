// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/x509v3.h>
#include <openssl/err.h>

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/error.h>
#include "object/datatype/function.h"
#include "object/datatype/integer.h"
#include "object/datatype/nil.h"

#include "ssl.h"

using namespace argon::object;
using namespace argon::module::ssl;

void CheckBlockingState(SSLSocket *socket) {

}

ARGON_METHOD5(sslsocket_, do_handshake, "", 0, false) {
    if (SSLSocketDoHandshake((SSLSocket *) self))
        return ARGON_OBJECT_NIL;

    return argon::vm::GetLastNonFatalError();
}

ARGON_METHOD5(sslsocket_, peercert, "", 1, false) {
    const auto *sock = (SSLSocket *) self;
    ArObject *ret;
    X509 *pc;

    bool binary;

    if (!CheckArgs("b:binary_form", func, argv, count))
        return nullptr;

    binary = ArBoolToBool((Bool *) argv[0]);

    if (!SSL_is_init_finished(sock->ssl))
        return ErrorFormat(type_value_error_, "handshake not done yet");

    pc = SSL_get_peer_certificate(sock->ssl);

    if (binary)
        ret = pc == nullptr ? BytesNew(0, true, false, true) : CertToDer(pc);
    else
        ret = pc == nullptr ? MapNew() : DecodeCert(pc);

    X509_free(pc);

    return ret;
}

ARGON_METHOD5(sslsocket_, pending, "", 0, false) {
    const auto *sock = (SSLSocket *) self;
    int length;

    length = SSL_pending(sock->ssl);
    //SSL_get_error(sock->ssl, length);

    if (length < 0)
        return SSLErrorSet();

    return IntegerNew(length);
}

ARGON_METHOD5(sslsocket_, read, "", 1, false) {
    auto *sock = (SSLSocket *) self;
    Bytes *bytes;
    unsigned char *buffer;
    ArSize size;
    ArSSize ret;

    if (!CheckArgs("i:bufsize", func, argv, count))
        return nullptr;

    size = ((Integer *) argv[0])->integer;

    if ((buffer = ArObjectNewRaw<unsigned char *>(size)) == nullptr)
        return nullptr;

    if ((ret = SSLSocketRead(sock, buffer, size)) < 0) {
        argon::memory::Free(buffer);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    if ((bytes = BytesNewHoldBuffer(buffer, ret, size, true)) == nullptr) {
        argon::memory::Free(buffer);
        return nullptr;
    }

    return ARGON_OBJECT_TUPLE_SUCCESS(bytes);
}

ARGON_METHOD5(sslsocket_, read_into, "", 1, false) {
    ArBuffer buffer{};
    auto *sock = (SSLSocket *) self;
    ArSSize ret;

    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::WRITE))
        return nullptr;

    if ((ret = SSLSocketRead(sock, buffer.buffer, buffer.len)) < 0) {
        BufferRelease(&buffer);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    BufferRelease(&buffer);

    return TupleNew("IA", ret, nullptr);
}

ARGON_METHOD5(sslsocket_, verify_client, "", 0, false) {
    if (SSL_verify_client_post_handshake(((SSLSocket *) self)->ssl) == 0)
        return SSLErrorGet();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslsocket_, unwrap, "", 0, false) {
    return SSLShutdown((SSLSocket *) self);
}

ARGON_METHOD5(sslsocket_, write, "", 2, false) {
    ArBuffer buffer{};
    auto *sock = (SSLSocket *) self;
    ArSize len;
    ArSSize ret;

    if (!CheckArgs("B:buffer,i?:nbytes", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    len = buffer.len;

    if (!IsNull(argv[1])) {
        len = ((Integer *) argv[1])->integer;

        if (len <= 0)
            return TupleNew("IA", 0, nullptr);

        if (len > buffer.len)
            len = buffer.len;
    }

    if ((ret = SSLSocketWrite(sock, buffer.buffer, len)) < 0) {
        BufferRelease(&buffer);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    BufferRelease(&buffer);

    return TupleNew("IA", ret, nullptr);
}

const NativeFunc sslsocket_methods[] = {
        sslsocket_do_handshake_,
        sslsocket_peercert_,
        sslsocket_pending_,
        sslsocket_read_,
        sslsocket_read_into_,
        sslsocket_verify_client_,
        sslsocket_unwrap_,
        sslsocket_write_,
        ARGON_METHOD_SENTINEL
};

ArObject *alpn_selected_get(const SSLSocket *self) {
    const unsigned char *out;
    unsigned int outlen;

    SSL_get0_alpn_selected(self->ssl, &out, &outlen);

    if (out == nullptr)
        ARGON_OBJECT_NIL;

    return StringNew((const char *) out, outlen);
}

Tuple *CipherToTuple(const SSL_CIPHER *cipher) {
    const char *name;
    const char *proto;
    int bits;

    name = SSL_CIPHER_get_name(cipher);
    proto = SSL_CIPHER_get_version(cipher);
    bits = SSL_CIPHER_get_bits(cipher, nullptr);

    return TupleNew("ssi", name, proto, bits);
}

ArObject *cipher_get(const SSLSocket *self) {
    const SSL_CIPHER *current;

    if ((current = SSL_get_current_cipher(self->ssl)) == nullptr)
        return ARGON_OBJECT_NIL;

    return CipherToTuple(current);
}

ArObject *compression_get(const SSLSocket *self) {
    const COMP_METHOD *comp_method;
    const char *name;

    comp_method = SSL_get_current_compression(self->ssl);
    if (comp_method == nullptr || COMP_get_type(comp_method) == NID_undef)
        return ARGON_OBJECT_NIL;

    name = OBJ_nid2sn(COMP_get_type(comp_method));
    if (name == nullptr)
        return ARGON_OBJECT_NIL;

    return StringNew(name);
}

ArObject *session_reused_get(const SSLSocket *self) {
    return BoolToArBool(SSL_session_reused(self->ssl));
}

ArObject *shared_cipher_get(const SSLSocket *self) {
    STACK_OF(SSL_CIPHER) *ciphers;
    Tuple *ret;
    int length;

    if ((ciphers = SSL_get_ciphers(self->ssl)) == nullptr)
        return ARGON_OBJECT_NIL;

    length = sk_SSL_CIPHER_num(ciphers);

    if ((ret = TupleNew(length)) == nullptr)
        return nullptr;

    for (int i = 0; i < length; i++) {
        auto *tmp = CipherToTuple(sk_SSL_CIPHER_value(ciphers, i));

        if (tmp == nullptr) {
            Release(ret);
            return nullptr;
        }

        TupleInsertAt(ret, i, tmp);
        Release(tmp);
    }

    return ret;
}

ArObject *version_get(const SSLSocket *self) {
    const char *version;

    if (!SSL_is_init_finished(self->ssl))
        return ARGON_OBJECT_NIL;

    version = SSL_get_version(self->ssl);

    return StringNew(version);
}

const NativeMember sslsocket_members[] = {
        ARGON_MEMBER_GETSET("alpn_selected", (NativeMemberGet) alpn_selected_get, nullptr,
                            NativeMemberType::STRING, true),
        ARGON_MEMBER_GETSET("cipher", (NativeMemberGet) cipher_get, nullptr, NativeMemberType::AROBJECT, true),
        ARGON_MEMBER_GETSET("compression", (NativeMemberGet) compression_get, nullptr, NativeMemberType::STRING, true),
        ARGON_MEMBER("hostname", offsetof(SSLSocket, hostname), NativeMemberType::AROBJECT, true),
        ARGON_MEMBER_GETSET("session_reused", (NativeMemberGet) session_reused_get, nullptr,
                            NativeMemberType::BOOL, true),
        ARGON_MEMBER_GETSET("shared_cipher", (NativeMemberGet) shared_cipher_get, nullptr,
                            NativeMemberType::AROBJECT, true),
        ARGON_MEMBER_GETSET("version", (NativeMemberGet) version_get, nullptr, NativeMemberType::STRING, true),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots sslsocket_obj = {
        sslsocket_methods,
        sslsocket_members,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void sslsocket_cleanup(SSLSocket *self) {
    Release(self->context);
    Release(self->hostname);
    Release(self->socket);

    SSL_free(self->ssl);
}

const TypeInfo SSLSocketType = {
        TYPEINFO_STATIC_INIT,
        "sslsocket",
        nullptr,
        sizeof(SSLSocket),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) sslsocket_cleanup,
        nullptr,
        nullptr,
        TypeInfo_IsTrue_True,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &sslsocket_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::ssl::type_sslsocket_ = &SSLSocketType;

ArSSize argon::module::ssl::SSLSocketRead(SSLSocket *socket, unsigned char *buffer, ArSize size) {
    ArSize count = 0;
    int retval;
    int err;

    CheckBlockingState(socket);

    ERR_clear_error();

    do {
        retval = SSL_read_ex(socket->ssl, buffer, size, &count);
        err = SSL_get_error(socket->ssl, retval);

        // TODO: check signals

        // TODO: if non blocking break
        if (err == SSL_ERROR_ZERO_RETURN && SSL_get_shutdown(socket->ssl) == SSL_RECEIVED_SHUTDOWN)
            return 0;

    } while (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);

    if (retval == 0)
        SSLErrorSet();

    return (ArSSize) count;
}

ArSSize argon::module::ssl::SSLSocketWrite(SSLSocket *socket, const unsigned char *buffer, ArSize size) {
    ArSize count = 0;
    int retval;
    int err;

    CheckBlockingState(socket);

    ERR_clear_error();

    do {
        retval = SSL_write_ex(socket->ssl, buffer, size, &count);
        err = SSL_get_error(socket->ssl, retval);

        // TODO: check signals

    } while (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);

    if (retval == 0)
        SSLErrorSet();

    return (ArSSize) count;
}

bool argon::module::ssl::SSLSocketDoHandshake(SSLSocket *socket) {
    int ret;
    int err;

    CheckBlockingState(socket);

    ERR_clear_error();

    do {
        ret = SSL_do_handshake(socket->ssl);
        err = SSL_get_error(socket->ssl, ret);

        // TODO: check signals

        // TODO: if non blocking break
    } while (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);

    if (ret < 1) {
        SSLErrorSet();
        return false;
    }

    return true;
}

bool ConfigureHostname(SSLSocket *socket, const char *name, ArSize len) {
    ASN1_OCTET_STRING *ip;
    if (len == 0 || *name == '.') {
        ErrorFormat(type_value_error_, "server_hostname cannot be an empty string or start with a leading dot");
        return false;
    }

    if ((ip = a2i_IPADDRESS(name)) == nullptr)
        ERR_clear_error();

    // SNI extension for non-IP hostname
    if (ip == nullptr && !SSL_set_tlsext_host_name(socket->ssl, name))
        goto ERROR;

    if (socket->context->check_hname) {
        X509_VERIFY_PARAM *param = SSL_get0_param(socket->ssl);
        if (ip == nullptr) {
            if (!X509_VERIFY_PARAM_set1_host(param, name, len))
                goto ERROR;
        } else {
            if (!X509_VERIFY_PARAM_set1_ip(param, ASN1_STRING_get0_data(ip), ASN1_STRING_length(ip)))
                goto ERROR;
        }
    }

    return true;

    ERROR:

    if (ip != nullptr)
        ASN1_OCTET_STRING_free(ip);

    SSLErrorSet();
    return false;
}

argon::module::socket::Socket *argon::module::ssl::SSLShutdown(SSLSocket *socket) {
    int attempt = 0;
    int ret = 0;

    CheckBlockingState(socket);

    while (attempt < 2 && ret <= 0) {
        if (attempt > 0)
            SSL_set_read_ahead(socket->ssl, 0);

        if ((ret = SSL_shutdown(socket->ssl)) < 0) {
            SSLErrorSet();
            return nullptr;
        }

        attempt++;
    }

    return IncRef(socket->socket);
}

SSLSocket *argon::module::ssl::SSLSocketNew(SSLContext *context, socket::Socket *socket,
                                            argon::object::String *hostname, bool server_side) {
    SSLSocket *sock;

    if (server_side && context->protocol == SSLProtocol::TLS_CLIENT)
        return (SSLSocket *) ErrorFormat(type_value_error_,
                                         "cannot create a server socket with a PROTO_TLS_CLIENT context");

    if (!server_side && context->protocol == SSLProtocol::TLS_SERVER)
        return (SSLSocket *) ErrorFormat(type_value_error_,
                                         "cannot create a server socket with a PROTO_TLS_SERVER context");

    if ((sock = ArObjectNew<SSLSocket>(RCType::INLINE, type_sslsocket_)) == nullptr)
        return nullptr;

    sock->context = IncRef(context);
    sock->socket = IncRef(socket);
    sock->hostname = IncRef(hostname);

    // Clear all SSL error
    ERR_clear_error();

    if ((sock->ssl = SSL_new(context->ctx)) == nullptr) {
        Release(sock);
        return (SSLSocket *) SSLErrorSet();
    }

    SSL_set_app_data(sock->ssl, sock);
    SSL_set_fd(sock->ssl, (int) socket->sock);

    SSL_set_mode(sock->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_AUTO_RETRY);

    if (context->post_handshake) {
        if (server_side) {
            int mode = SSL_get_verify_mode(sock->ssl);
            if (mode & SSL_VERIFY_PEER) {
                int (*verify_cb)(int, X509_STORE_CTX *);
                verify_cb = SSL_get_verify_callback(sock->ssl);
                mode |= SSL_VERIFY_POST_HANDSHAKE;
                SSL_set_verify(sock->ssl, mode, verify_cb);
            } else
                SSL_set_post_handshake_auth(sock->ssl, 1);
        }
    }

    if (!IsNull(hostname) && !ConfigureHostname(sock, (const char *) hostname->buffer, hostname->len)) {
        Release(sock);
        return nullptr;
    }

    // TODO: Check for non-blocking mode

    server_side ? SSL_set_accept_state(sock->ssl) : SSL_set_connect_state(sock->ssl);

    sock->protocol = server_side ? SSLProtocol::TLS_SERVER : SSLProtocol::TLS_CLIENT;

    return sock;
}