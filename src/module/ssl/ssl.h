// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_SSL_H_
#define ARGON_MODULE_SSL_H_

#include <openssl/ssl.h>

#include <utils/macros.h>

#include <object/datatype/string.h>
#include <object/datatype/map.h>
#include <object/arobject.h>

#include <module/socket/socket.h>
#include "object/datatype/bytes.h"

namespace argon::module::ssl {
    enum class SSLProtocol : int {
        TLS,
        TLS_CLIENT,
        TLS_SERVER
    };

    enum class SSLVerify : int {
        CERT_NONE,
        CERT_OPTIONAL,
        CERT_REQUIRED
    };

    struct SSLContext : argon::object::ArObject {
        argon::object::ArObject *sni_callback;
        SSL_CTX *ctx;

        SSLProtocol protocol;
        SSLVerify verify_mode;
        unsigned int hostflags;
        bool check_hname;
        bool post_handshake;
    };

    extern const argon::object::TypeInfo *type_sslcontext_;

    struct SSLSocket : argon::object::ArObject {
        SSLContext *context;
        socket::Socket *socket;
        argon::object::String *hostname;

        SSL *ssl;

        SSLProtocol protocol;
    };

    extern const argon::object::TypeInfo *type_sslsocket_;

    argon::object::ArObject *SSLErrorGet();

    argon::object::ArObject *SSLErrorGet(SSLSocket *socket, int ret);

    argon::object::ArObject *SSLErrorSet();

    argon::object::ArObject *SSLErrorSet(SSLSocket *socket, int ret);

    argon::object::Bytes *CertToDer(X509 *cert);

    argon::object::Map *DecodeCert(X509 *cert);

#ifdef _ARGON_PLATFORM_WINDOWS

    argon::object::Tuple *EnumWindowsCert(const char *store_name);

#endif

    socket::Socket *SSLShutdown(SSLSocket *socket);

    SSLContext *SSLContextNew(SSLProtocol protocol);

    SSLSocket *SSLSocketNew(SSLContext *context, socket::Socket *socket,
                            argon::object::String *hostname, bool server_side);

    argon::object::ArSSize SSLSocketRead(SSLSocket *socket, unsigned char *buffer, argon::object::ArSize size);

    argon::object::ArSSize SSLSocketWrite(SSLSocket *socket, const unsigned char *buffer, argon::object::ArSize size);

    bool SSLSocketDoHandshake(SSLSocket *socket);

} // namespace argon::module

#endif // !ARGON_MODULE_SSL_H_
