// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "ssl.h"

#include <openssl/err.h>

#include <utils/macros.h>

#include <object/datatype/error.h>
#include "object/datatype/nil.h"

#include "ssl.h"

using namespace argon::object;
using namespace argon::module::ssl;

ARGON_FUNCTION5(sslcontext_, new, "", 1, false) {
    if (!CheckArgs("i:protocol", func, argv, count))
        return nullptr;

    return nullptr;
}

ARGON_METHOD5(sslcontext_, load_certs_default, "", 1, false) {
    auto *ctx = (SSLContext *) self;

#ifdef _ARGON_PLATFORM_WINDOWS
    // TODO: WINDOWS
#else
    if (!SSL_CTX_set_default_verify_paths(ctx->ctx)) {
        // TODO: error
        return nullptr;
    }
#endif

    return ARGON_OBJECT_NIL;
}

const NativeFunc sslcontext_methods[] = {
        sslcontext_new_,
        sslcontext_load_certs_default_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots sslcontext_obj = {
        sslcontext_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void sslcontext_cleanup(SSLContext *self) {
    SSL_CTX_free(self->ctx);
}

const TypeInfo SSLContextType = {
        TYPEINFO_STATIC_INIT,
        "sslcontext",
        nullptr,
        sizeof(SSLContext),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) sslcontext_cleanup,
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
        &sslcontext_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::ssl::type_sslcontext_ = &SSLContextType;

SSLContext *SSLContextNew(SSLProtocol protocol) {
    const SSL_METHOD *method = nullptr;
    SSLContext *ctx;

    switch (protocol) {
        case SSLProtocol::TLS:
            method = TLS_method();
            break;
        case SSLProtocol::TLS_CLIENT:
            method = TLS_client_method();
            break;
        case SSLProtocol::TLS_SERVER:
            method = TLS_server_method();
            break;
    }

    if (method == nullptr)
        return (SSLContext *) ErrorFormat(type_value_error_, "invalid protocol %i", protocol);

    if ((ctx = ArObjectNew<SSLContext>(RCType::INLINE, type_sslcontext_)) == nullptr)
        return nullptr;

    if ((ctx->ctx = SSL_CTX_new(method)) == nullptr) {
        Release(ctx);
        // TODO: ERROR
        return nullptr;
    }

    SSL_CTX_set_default_verify_paths(ctx->ctx);

    return ctx;
}