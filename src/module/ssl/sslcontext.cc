// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/x509v3.h>
#include <openssl/err.h>

#include <utils/macros.h>

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>

#include <module/socket/socket.h>

#include "ssl.h"

using namespace argon::object;
using namespace argon::module::ssl;

static bool MinMaxProtoVersion(SSLContext *context, unsigned int opt, bool set_max) {
    long result;

    switch (context->protocol) {
        case SSLProtocol::TLS:
        case SSLProtocol::TLS_CLIENT:
        case SSLProtocol::TLS_SERVER:
            break;
        default:
            ErrorFormat(type_value_error_, "this context doesn't support modification of highest and lowest version");
            return false;
    }

    switch (opt) {
        case SSL3_VERSION:
        case TLS1_VERSION:
        case TLS1_1_VERSION:
        case TLS1_2_VERSION:
        case TLS1_3_VERSION:
            break;
        default:
            ErrorFormat(type_value_error_, "unsupported TLS/SSL version 0x%x", opt);
            return false;
    }

    result = set_max ? SSL_CTX_set_max_proto_version(context->ctx, opt) :
             SSL_CTX_set_min_proto_version(context->ctx, opt);

    if (result == 0) {
        ErrorFormat(type_value_error_, "unsupported protocol version 0x%x", opt);
        return false;
    }

    return true;
}

static bool SetVerifyMode(SSLContext *context, SSLVerify mode) {
    int (*callback)(int, X509_STORE_CTX *);
    int sslmode;

    switch (mode) {
        case SSLVerify::CERT_NONE:
            sslmode = SSL_VERIFY_NONE;
            break;
        case SSLVerify::CERT_OPTIONAL:
            sslmode = SSL_VERIFY_PEER;
            break;
        case SSLVerify::CERT_REQUIRED:
            sslmode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
            break;
        default:
            ErrorFormat(type_value_error_, "invalid value for VerifyMode");
            return false;
    }

    callback = SSL_CTX_get_verify_callback(context->ctx);
    SSL_CTX_set_verify(context->ctx, sslmode, callback);

    context->verify_mode = mode;

    return true;
}

ARGON_FUNCTION5(sslcontext_, new, "", 1, false) {
    if (!CheckArgs("i:protocol", func, argv, count))
        return nullptr;

    return SSLContextNew((SSLProtocol) ((Integer *) argv[0])->integer);
}

ARGON_METHOD5(sslcontext_, load_cacerts, "", 1, false) {
    const auto *ctx = (SSLContext *) self;
    STACK_OF(X509_OBJECT) *objs;
    X509_STORE *store;
    ArObject *tmp;
    List *ret;

    bool binary;

    if (!CheckArgs("b:binary_form", func, argv, count))
        return nullptr;

    binary = ArBoolToBool((Bool *) argv[0]);

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    store = SSL_CTX_get_cert_store(ctx->ctx);
    objs = X509_STORE_get0_objects(store);

    for (int i = 0; i < sk_X509_OBJECT_num(objs); i++) {
        const X509_OBJECT *obj;
        X509 *cert;

        obj = sk_X509_OBJECT_value(objs, i);
        if (X509_OBJECT_get_type(obj) != X509_LU_X509)
            continue; // not a x509 cert

        cert = X509_OBJECT_get0_X509(obj);
        if (!X509_check_ca(cert))
            continue;

        if (binary)
            tmp = CertToDer(cert);
        else
            tmp = DecodeCert(cert);

        if (tmp == nullptr) {
            Release(ret);
            return nullptr;
        }

        if (!ListAppend(ret, tmp)) {
            Release(ret);
            return nullptr;
        }

        Release(tmp);
    }

    return ret;
}

ARGON_METHOD5(sslcontext_, load_cafile, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("s:cafile", func, argv, count))
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

    if (!CheckArgs("s:capath", func, argv, count))
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
    if (!SSL_CTX_set_default_verify_paths(((SSLContext *) self)->ctx))
        return SSLErrorSet();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, load_dh_params, "", 1, false) {
    auto *ctx = (SSLContext *) self;
    FILE *file;
    DH *dh;

    if (!CheckArgs("s:filepath", func, argv, count))
        return nullptr;

    errno = 0;
    if ((file = fopen((const char *) ((String *) argv[0])->buffer, "rb")) == nullptr)
        return ErrorSetFromErrno();

    dh = PEM_read_DHparams(file, nullptr, nullptr, nullptr);
    fclose(file);

    if (dh == nullptr) {
        if (errno != 0) {
            ERR_clear_error();
            return ErrorSetFromErrno();
        }

        return SSLErrorSet();
    }

    if (!SSL_CTX_set_tmp_dh(ctx->ctx, dh)) {
        DH_free(dh);
        return SSLErrorSet();
    }

    DH_free(dh);

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, load_paths_default, "", 1, false) {
    if (!SSL_CTX_set_default_verify_paths(((SSLContext *) self)->ctx))
        return SSLErrorSet();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, make_stats, "", 0, false) {
#define ADD_STAT(SSLNAME, KEY)                                          \
    if((tmp = IntegerNew(SSL_CTX_sess_##SSLNAME(ctx->ctx))) == nullptr) \
        goto ERROR;                                                     \
    if(!MapInsertRaw(map, (KEY), tmp)) {                                \
        Release(tmp);                                                   \
        goto ERROR;                                                     \
    } Release(tmp)

    auto *ctx = (SSLContext *) self;
    Integer *tmp;
    Map *map;

    if ((map = MapNew()) == nullptr)
        return nullptr;

    ADD_STAT(number, "number");
    ADD_STAT(connect, "connect");
    ADD_STAT(connect_good, "connect_good");
    ADD_STAT(connect_renegotiate, "connect_renegotiate");
    ADD_STAT(accept, "accept");
    ADD_STAT(accept_good, "accept_good");
    ADD_STAT(accept_renegotiate, "accept_renegotiate");
    ADD_STAT(accept, "accept");
    ADD_STAT(hits, "hits");
    ADD_STAT(misses, "misses");
    ADD_STAT(timeouts, "timeouts");
    ADD_STAT(cache_full, "cache_full");

    return map;

    ERROR:
    Release(tmp);
    Release(map);
    return nullptr;
#undef ADD_STAT
}

ARGON_METHOD5(sslcontext_, set_check_hostname, "", 1, false) {
    auto *ctx = (SSLContext *) self;
    bool check;

    if (!CheckArgs("b:check", func, argv, count))
        return nullptr;

    check = ArBoolToBool((Bool *) argv[0]);

    if (check && SSL_CTX_get_verify_mode(ctx->ctx) == SSL_VERIFY_NONE)
        SetVerifyMode(ctx, SSLVerify::CERT_REQUIRED);

    ctx->check_hname = check;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_ciphers, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("s:cipher", func, argv, count))
        return nullptr;

    if (SSL_CTX_set_cipher_list(ctx->ctx, (const char *) ((String *) argv[0])->buffer) == 0) {
        SSLErrorSet();
        ERR_clear_error();
        return nullptr;
    }

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_max_version, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("i:version", func, argv, count))
        return nullptr;

    if (!MinMaxProtoVersion(ctx, (unsigned int) ((Integer *) argv[0])->integer, true))
        return nullptr;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_min_version, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("i:version", func, argv, count))
        return nullptr;

    if (!MinMaxProtoVersion(ctx, (unsigned int) ((Integer *) argv[0])->integer, false))
        return nullptr;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_num_tickets, "", 1, false) {
    auto *ctx = (SSLContext *) self;
    unsigned long ticket;

    if (!CheckArgs("i:ticket", func, argv, count))
        return nullptr;

    ticket = ((Integer *) argv[0])->integer;

    if (ctx->protocol != SSLProtocol::TLS_SERVER)
        return ErrorFormat(type_value_error_, "not a server context");

    if (SSL_CTX_set_num_tickets(ctx->ctx, ticket) != 1)
        return ErrorFormat(type_value_error_, "failed to set num tickets");

    return ARGON_OBJECT_NIL;
}

static int ServernameCallback(SSL *ssl, int *al, void *args) {
    ArObject *call_arg[3] = {};
    auto *ctx = (SSLContext *) args;
    const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    String *name = nullptr;
    SSLSocket *sock;
    ArObject *result;

    if (IsNull(ctx->sni_callback))
        return SSL_TLSEXT_ERR_OK;

    sock = (SSLSocket *) SSL_get_app_data(ssl);

    if (sock == nullptr) {
        *al = SSL_AD_INTERNAL_ERROR;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    call_arg[0] = ctx;
    call_arg[1] = sock;
    call_arg[2] = NilVal;

    if (servername != nullptr) {
        if ((name = StringNew(servername)) == nullptr) {
            *al = SSL_AD_INTERNAL_ERROR;
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }

        call_arg[2] = name;
    }

    result = argon::vm::Call(ctx->sni_callback, 3, call_arg);
    Release(name);

    if (result == nullptr) {
        *al = SSL_AD_HANDSHAKE_FAILURE;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    if (AR_TYPEOF(result, type_nil_)) {
        Release(result);
        return SSL_TLSEXT_ERR_OK;
    }

    *al = SSL_AD_INTERNAL_ERROR;

    if (AR_TYPEOF(result, type_integer_))
        *al = (int) ((Integer *) result)->integer;

    Release(result);

    return *al != SSL_AD_INTERNAL_ERROR ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_ALERT_FATAL;
}

ARGON_METHOD5(sslcontext_, set_sni, "", 1, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("?*:callback", func, argv, count, type_function_))
        return nullptr;

    if (ctx->protocol == SSLProtocol::TLS_CLIENT)
        return ErrorFormat(type_value_error_, "sni callback cannot be set on TLS_CLIENT");

    if (IsNull(argv[0])) {
        SSL_CTX_set_tlsext_servername_callback(ctx->ctx, nullptr);
        Release(&ctx->sni_callback);
        return ARGON_OBJECT_NIL;
    }

    ctx->sni_callback = IncRef(argv[0]);

    SSL_CTX_set_tlsext_servername_callback(ctx->ctx, ServernameCallback);
    SSL_CTX_set_tlsext_servername_arg(ctx->ctx, self);

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_verify, "", 1, false) {
    auto *ctx = (SSLContext *) self;
    SSLVerify flag;

    if (!CheckArgs("i:verify", func, argv, count))
        return nullptr;

    flag = (SSLVerify) ((Integer *) argv[0])->integer;

    if (flag == SSLVerify::CERT_NONE && ctx->check_hname)
        return ErrorFormat(type_value_error_, "cannot set verify mode to CERT_NONE when check hostname is enabled");

    if (!SetVerifyMode(ctx, flag))
        return nullptr;

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, set_verify_flags, "", 1, false) {
    auto *ctx = (SSLContext *) self;
    X509_VERIFY_PARAM *param;
    unsigned long clear;
    unsigned long flags;
    unsigned long new_flags;
    unsigned long set;

    if (!CheckArgs("i:flags", func, argv, count))
        return nullptr;

    new_flags = ((Integer *) argv[0])->integer;

    param = SSL_CTX_get0_param(ctx->ctx);
    flags = X509_VERIFY_PARAM_get_flags(param);
    clear = flags & ~new_flags;
    set = ~flags & new_flags;

    if (clear && !X509_VERIFY_PARAM_clear_flags(param, clear))
        return SSLErrorSet();

    if (set && !X509_VERIFY_PARAM_set_flags(param, set))
        return SSLErrorSet();

    return ARGON_OBJECT_NIL;
}

ARGON_METHOD5(sslcontext_, wrap, "", 3, false) {
    auto *ctx = (SSLContext *) self;

    if (!CheckArgs("*:sock,b:server_side,s?:hostname", func, argv, count, argon::module::socket::type_socket_))
        return nullptr;

    return SSLSocketNew(ctx, (argon::module::socket::Socket *) argv[0], (String *) argv[2],
                        ArBoolToBool((Bool *) argv[1]));
}

const NativeFunc sslcontext_methods[] = {
        sslcontext_new_,
        sslcontext_load_cacerts_,
        sslcontext_load_cafile_,
        sslcontext_load_capath_,
        sslcontext_load_cert_chain_,
        sslcontext_load_certs_default_,
        sslcontext_load_dh_params_,
        sslcontext_load_paths_default_,
        sslcontext_make_stats_,
        sslcontext_set_check_hostname_,
        sslcontext_set_ciphers_,
        sslcontext_set_max_version_,
        sslcontext_set_min_version_,
        sslcontext_set_num_tickets_,
        sslcontext_set_sni_,
        sslcontext_set_verify_,
        sslcontext_set_verify_flags_,
        sslcontext_wrap_,
        ARGON_METHOD_SENTINEL
};

ArObject *security_level_get(const SSLContext *context) {
    return IntegerNew(SSL_CTX_get_security_level(context->ctx));
}

ArObject *session_ticket_get(const SSLContext *context) {
    // TODO: need ulong
    return IntegerNew(SSL_CTX_get_num_tickets(context->ctx));
}

const NativeMember sslcontext_members[] = {
        ARGON_MEMBER("check_hostname", offsetof(SSLContext, check_hname), NativeMemberType::BOOL, true),
        ARGON_MEMBER("protocol", offsetof(SSLContext, protocol), NativeMemberType::INT, true),
        ARGON_MEMBER_GETSET("security_level", (NativeMemberGet) security_level_get, nullptr,
                            NativeMemberType::INT, true),
        ARGON_MEMBER_GETSET("session_ticket", (NativeMemberGet) session_ticket_get, nullptr,
                            NativeMemberType::INT, true),
        ARGON_MEMBER("sni_callback", offsetof(SSLContext, sni_callback), NativeMemberType::AROBJECT, true),
        ARGON_MEMBER("verify_mode", offsetof(SSLContext, verify_mode), NativeMemberType::INT, true),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots sslcontext_obj = {
        sslcontext_methods,
        sslcontext_members,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void sslcontext_cleanup(SSLContext *self) {
    SSL_CTX_free(self->ctx);
    Release(self->sni_callback);
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
    X509_VERIFY_PARAM *params;
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

    ctx->sni_callback = nullptr;
    ctx->protocol = protocol;

    ctx->verify_mode = SSLVerify::CERT_NONE;
    ctx->hostflags = X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS;

    if (protocol == SSLProtocol::TLS_CLIENT) {
        ctx->check_hname = true;
        SetVerifyMode(ctx, SSLVerify::CERT_REQUIRED);
    } else {
        ctx->check_hname = false;
        SetVerifyMode(ctx, SSLVerify::CERT_NONE);
    }

#ifdef SSL_MODE_RELEASE_BUFFERS
    /*
     * When we no longer need a read buffer or a write buffer for a given SSL,
     * then release the memory we were using to hold it.
     * Using this flag can save around 34k per idle SSL connection.
     * This flag has no effect on SSL v2 connections, or on DTLS connections.
     */
    SSL_CTX_set_mode(ctx->ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

    /*
     * When X509_V_FLAG_TRUSTED_FIRST is set, which is always the case since OpenSSL 1.1.0,
     * construction of the certificate chain in X509_verify_cert(3) searches the trust store
     * for issuer certificates before searching the provided untrusted certificates.
     */
    params = SSL_CTX_get0_param(ctx->ctx);
    X509_VERIFY_PARAM_set_flags(params, X509_V_FLAG_TRUSTED_FIRST);
    X509_VERIFY_PARAM_set_hostflags(params, ctx->hostflags);

    ctx->post_handshake = false;
    SSL_CTX_set_post_handshake_auth(ctx->ctx, ctx->post_handshake);

    return ctx;
}