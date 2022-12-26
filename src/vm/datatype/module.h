// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_DATATYPE_MODULE_H_
#define ARGON_DATATYPE_MODULE_H_

#include "arstring.h"
#include "namespace.h"
#include "arobject.h"

namespace argon::vm::datatype {

#define MODULE_ATTRIBUTE_DEFAULT (AttributeFlag::CONST|AttributeFlag::PUBLIC)

    using ModuleInitFn = bool (*)(struct Module *);
    using ModuleFiniFn = bool (*)(struct Module *);

    struct Module {
        AROBJ_HEAD;

        Namespace *ns;

        ModuleFiniFn fini;
    };
    extern const TypeInfo *type_module_;

    struct ModuleEntry {
        const char *name;

        union {
            ArObject *object;
            FunctionDef *func;
        } prop;

        bool func;
        AttributeFlag flags;
    };

#define ARGON_MODULE_SENTINEL {nullptr, nullptr, false, (argon::vm::datatype::AttributeFlag)0}

    struct ModuleInit {
        const char *name;
        const char *doc;

        const ModuleEntry *bulk;

        ModuleInitFn init;
        ModuleFiniFn fini;
    };

    ArObject *ModuleLookup(const Module *mod, const char *key, AttributeProperty *out_prop);

    bool ModuleAddObject(Module *mod, const char *key, ArObject *object, AttributeFlag flags);

    Module *ModuleNew(const ModuleInit *init);

    Module *ModuleNew(String *name, String *doc);

    Module *ModuleNew(const char *name, const char *doc);

} // namespace argon::vm::datatype

#endif // !ARGON_DATATYPE_MODULE_H_
