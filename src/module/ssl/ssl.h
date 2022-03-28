// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_SSL_H_
#define ARGON_MODULE_SSL_H_

#include <openssl/ssl.h>

#include <object/datatype/string.h>
#include <object/arobject.h>

#include <module/socket/socket.h>

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

    argon::object::ArObject *SSLErrorSet();

    SSLContext *SSLContextNew(SSLProtocol protocol);

    SSLSocket *SSLSocketNew(SSLContext *context, socket::Socket *socket,
                            argon::object::String *hostname, bool server_side);

} // namespace argon::module

#endif // !ARGON_MODULE_SSL_H_
