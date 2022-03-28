// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/err.h>

#include "object/datatype/bool.h"
#include <object/datatype/error.h>
#include "object/datatype/function.h"
#include "object/datatype/integer.h"
#include "object/datatype/nil.h"

#include "ssl.h"

using namespace argon::object;
using namespace argon::module::ssl;

const NativeFunc sslsocket_methods[] = {
        ARGON_METHOD_SENTINEL
};

const ObjectSlots sslsocket_obj = {
        sslsocket_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const TypeInfo SSLSocketType = {
        TYPEINFO_STATIC_INIT,
        "sslsocket",
        nullptr,
        sizeof(SSLSocket),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
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

    // BIO?

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

    if (hostname != nullptr) {
        // todo: configure hostname
    }

    // Check for non-blocking mode

    server_side ? SSL_set_accept_state(sock->ssl) : SSL_set_connect_state(sock->ssl);

    sock->protocol = server_side ? SSLProtocol::TLS_SERVER : SSLProtocol::TLS_CLIENT;

    return sock;
}