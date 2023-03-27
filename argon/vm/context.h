// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONTEXT_H_
#define ARGON_VM_CONTEXT_H_

#include <argon/vm/importer/import.h>

#include <argon/vm/mod/modules.h>

#include <argon/vm/config.h>

namespace argon::vm {
    struct Context {
        Config *global_config;

        importer::Import *imp;

        datatype::Module *builtins;
    };

    Context *ContextNew(Config *global_config);

    void ContextDel(Context *context);

} // namespace argon::vm

#endif // !ARGON_VM_CONTEXT_H_
