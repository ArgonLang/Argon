// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONTEXT_H_
#define ARGON_VM_CONTEXT_H_

#include <object/arobject.h>
#include <object/datatype/module.h>

#include <module/io/io.h>

#include "import.h"

namespace argon::vm {
    struct Context {
        argon::object::Module *bltins;
        argon::object::Module *runtime;

        Import *import;
    };

    Context *ContextNew();

    argon::object::ArObject *ContextRuntimeGetProperty(const char *key, const argon::object::TypeInfo *info);

    void ContextDel(Context *context);
}

#endif // !ARGON_VM_CONTEXT_H_
