// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONTEXT_H_
#define ARGON_VM_CONTEXT_H_

#include <vm/importer/import.h>

#include <vm/mod/modules.h>

namespace argon::vm {
    struct Context {
        importer::Import *imp;

        datatype::Module *builtins;
    };

    Context *ContextNew();

    void ContextDel(Context *context);

} // namespace argon::vm

#endif // !ARGON_VM_CONTEXT_H_
