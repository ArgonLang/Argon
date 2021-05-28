// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include "error.h"

using namespace argon::object;
using namespace argon::module;

bool error_init(Module *self) {
    return NamespaceMerge(self->module_ns, (Namespace *) error_types);
}

const ModuleInit module_error = {
        "_error",
        "This module exports all Argon native errors, if you are looking for error handling features, "
        "you should import error, not _error!",
        nullptr,
        error_init,
        nullptr
};

Module *argon::module::ErrorNew() {
    return ModuleNew(&module_error);
}
