// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <vm/runtime.h>
#include <object/datatype/error.h>

#include <module/modules.h>

#include "ssl.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::ssl;

ArObject *argon::module::ssl::SSLErrorGet() {
    char buf[256] = {};

    // TODO: error type
    if (ERR_error_string(ERR_get_error(), buf) == nullptr)
        return ErrorFormatNoPanic(type_os_error_, "unknown error");

    return ErrorFormatNoPanic(type_os_error_, "%s", buf);
}

ArObject *argon::module::ssl::SSLErrorSet() {
    ArObject *err = SSLErrorGet();

    argon::vm::Panic(err);
    Release(err);

    return nullptr;
}

const PropertyBulk ssl_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("sslcontext", type_sslcontext_),
        MODULE_EXPORT_SENTINEL
};

bool SSLInit(Module *self) {
    if (!TypeInit((TypeInfo *) type_sslcontext_, nullptr))
        return false;

    SSL_load_error_strings();
    SSL_library_init();
    return true;
}

const ModuleInit module_ssl = {
        "_ssl",
        "",
        ssl_bulk,
        SSLInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_ssl_ = &module_ssl;
