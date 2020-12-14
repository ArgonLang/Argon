// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULES_RUNTIME_H_
#define ARGON_MODULES_RUNTIME_H_

#include "io/io.h"
#include <object/datatype/module.h>

namespace argon::module {
    argon::object::Module *RuntimeNew();

    argon::object::ArObject *RuntimeGetProperty(const char *key, const argon::object::TypeInfo *info);
} // namespace argon::module


#endif // !ARGON_MODULES_RUNTIME_H_
