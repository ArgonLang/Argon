// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <object/datatype/error.h>

#include <module/modules.h>

#include "ssl.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::ssl;

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
