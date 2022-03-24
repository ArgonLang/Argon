// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "ssl.h"

#include <openssl/err.h>

#include <utils/macros.h>

#include <vm/runtime.h>

#include <object/datatype/error.h>
#include "object/datatype/function.h"
#include "object/datatype/integer.h"
#include "object/datatype/nil.h"

#include "ssl.h"

using namespace argon::object;
using namespace argon::module::ssl;

ARGON_FUNCTION5(sslcontext_, new, "", 1, false) {
    if (!CheckArgs("i:protocol", func, argv, count))
        return nullptr;

    return SSLContextNew((SSLProtocol) ((Integer *) argv[0])->integer);
}

ARGON_METHOD5(sslcontext_, load_cafile, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("s:cafile", func, argv, count, type_function_))
        return nullptr;

    if (SSL_CTX_load_verify_locations(ctx->ctx, (const char *) ((String *) argv[0])->buffer, nullptr) != 1) {
        if (!argon::vm::IsPanicking())
            errno != 0 ? ErrorSetFromErrno() : SSLErrorSet();

        return nullptr;
    }

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, load_capath, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("s:capath", func, argv, count, type_function_))
        return nullptr;

    if (SSL_CTX_load_verify_locations(ctx->ctx, nullptr, (const char *) ((String *) argv[0])->buffer) != 1) {
        if (!argon::vm::IsPanicking())
            errno != 0 ? ErrorSetFromErrno() : SSLErrorSet();

        return nullptr;
    }

    return ARGON_OBJECT_NIL;
}

static int PasswordCallback(char *buf, int size, int rwflag, void *userdata) {
    auto *obj = (ArObject *) userdata;
    String *ret;
    int len;

    if (AR_TYPEOF(obj, type_function_)) {
        ret = (String *) argon::vm::Call(obj, 0, nullptr);
        if (ret == nullptr)
            return -1;
    } else
        ret = (String *) IncRef(obj);

    if (!AR_TYPEOF(ret, type_string_)) {
        ErrorFormat(type_type_error_, "callback must return a string not '%s'", AR_TYPE_NAME(ret));
        Release(ret);
        return -1;
    }

    len = (int) ret->len;

    if (len > size) {
        ErrorFormat(type_value_error_, "password cannot be longer than %d bytes", size);
        Release(ret);
        return -1;
    }

    argon::memory::MemoryCopy(buf, ret->buffer, len);
    Release(ret);

    return len;
}

ARGON_METHOD5(sslcontext_, load_cert_chain, "", 3, false) {
    const auto *ctx = (SSLContext *) self;
    const auto *certfile = (String *) argv[0];
    const auto *keyfile = (String *) argv[1];
    auto *callback = argv[2];
    pem_password_cb *orig_pwd_cb;
    void *orig_pwd_userdata;

    if (!CheckArgs("s:certfile,s?:keyfile,s*?:password", func, argv, count, type_function_))
        return nullptr;

    orig_pwd_cb = SSL_CTX_get_default_passwd_cb(ctx->ctx);
    orig_pwd_userdata = SSL_CTX_get_default_passwd_cb_userdata(ctx->ctx);

    if (IsNull(keyfile))
        keyfile = certfile;

    if (!IsNull(callback)) {
        if (!AR_TYPEOF(callback, type_string_) && !AR_TYPEOF(callback, type_function_))
            return ErrorFormat(type_type_error_, "password should be a string or callable");

        SSL_CTX_set_default_passwd_cb(ctx->ctx, PasswordCallback);
        SSL_CTX_set_default_passwd_cb_userdata(ctx->ctx, callback);
    }

    errno = 0;
    if (SSL_CTX_use_certificate_chain_file(ctx->ctx, (const char *) certfile->buffer) != 1) {
        if (!argon::vm::IsPanicking())
            errno != 0 ? ErrorSetFromErrno() : SSLErrorSet();
        goto ERROR;
    }

    errno = 0;
    if (SSL_CTX_use_PrivateKey_file(ctx->ctx, (const char *) keyfile->buffer, SSL_FILETYPE_PEM) != 1) {
        if (!argon::vm::IsPanicking())
            errno != 0 ? ErrorSetFromErrno() : SSLErrorSet();
        goto ERROR;
    }

    if (SSL_CTX_check_private_key(ctx->ctx) != 1) {
        SSLErrorSet();
        goto ERROR;
    }

    SSL_CTX_set_default_passwd_cb(ctx->ctx, orig_pwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx->ctx, orig_pwd_userdata);

    return ARGON_OBJECT_NIL;

    ERROR:
    SSL_CTX_set_default_passwd_cb(ctx->ctx, orig_pwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx->ctx, orig_pwd_userdata);
    return nullptr;
}

ARGON_METHOD5(sslcontext_, load_certs_default, "", 1, false) {
    auto *ctx = (SSLContext *) self;

#ifdef _ARGON_PLATFORM_WINDOWS
    // TODO: WINDOWS
#else
    if (!SSL_CTX_set_default_verify_paths(ctx->ctx)) {
        return SSLErrorSet();
    }
#endif

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_ciphers, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("s:cipher", func, argv, count, type_function_))
        return nullptr;

    if (SSL_CTX_set_cipher_list(ctx->ctx, (const char *) ((String *) argv[0])->buffer) == 0) {
        SSLErrorSet();
        ERR_clear_error();
        return nullptr;
    }

    return ARGON_OBJECT_NIL;
}

const NativeFunc sslcontext_methods[] = {
        sslcontext_new_,
        sslcontext_load_cafile_,
        sslcontext_load_capath_,
        sslcontext_load_cert_chain_,
        sslcontext_load_certs_default_,
        sslcontext_set_ciphers_,
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

SSLContext *argon::module::ssl::SSLContextNew(SSLProtocol protocol) {
    const SSL_METHOD *method;
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
        default:
            return (SSLContext *) ErrorFormat(type_value_error_, "invalid protocol %i", protocol);
    }

    if ((ctx = ArObjectNew<SSLContext>(RCType::INLINE, type_sslcontext_)) == nullptr)
        return nullptr;

    if ((ctx->ctx = SSL_CTX_new(method)) == nullptr) {
        Release(ctx);
        return (SSLContext *) SSLErrorSet();
    }

    return ctx;
}