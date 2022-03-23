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

    struct SSLContext : argon::object::ArObject {
        SSL_CTX *ctx;
    };

    extern const argon::object::TypeInfo *type_sslcontext_;
} // namespace argon::module

#endif // !ARGON_MODULE_SSL_H_
