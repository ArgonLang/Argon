// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <module/modules.h>

#include "ssl.h"

using namespace argon::object;
using namespace argon::module;

bool SSLInit(Module *self) {
    SSL_load_error_strings();
    SSL_library_init();
    return true;
}

const ModuleInit module_ssl = {
        "_ssl",
        "",
        nullptr,
        SSLInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_ssl_ = &module_ssl;
