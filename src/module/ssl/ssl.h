// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_SSL_H_
#define ARGON_MODULE_SSL_H_

#include <openssl/ssl.h>

#include <object/arobject.h>

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
        bool check_hname;
    };

    extern const argon::object::TypeInfo *type_sslcontext_;

    argon::object::ArObject *SSLErrorGet();

    argon::object::ArObject *SSLErrorSet();

    SSLContext *SSLContextNew(SSLProtocol protocol);

} // namespace argon::module

#endif // !ARGON_MODULE_SSL_H_
