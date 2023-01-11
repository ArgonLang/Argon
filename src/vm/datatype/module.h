// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_DATATYPE_MODULE_H_
#define ARGON_DATATYPE_MODULE_H_

#include "arstring.h"
#include "namespace.h"
#include "arobject.h"

namespace argon::vm::datatype {

#define ARGON_MODULE_INIT(__m_init)                             \
    extern "C" const struct ModuleInit *__ar_module_init(void)  \
    { return (&__m_init); }

    constexpr const char *kModuleInitFnName = "__ar_module_init";

    using ModuleNativeInitFn = const struct ModuleInit *(*)();

    using ModuleInitFn = bool (*)(struct Module *);
    using ModuleFiniFn = bool (*)(struct Module *);

    struct Module {
        AROBJ_HEAD;

        Namespace *ns;

        ModuleFiniFn fini;

        ModuleFiniFn _nfini;

        uintptr_t _dlhandle;
    };
    extern const TypeInfo *type_module_;

    struct ModuleEntry {
        const char *name;

        union {
            ArObject *object;
            const FunctionDef *func;
        } prop;

        bool func;
        AttributeFlag flags;
    };

#define MODULE_ATTRIBUTE_DEFAULT (AttributeFlag::CONST | AttributeFlag::PUBLIC)

#define MODULE_EXPORT_FUNCTION(fn_native)                                       \
    {(fn_native).name, {.func=&(fn_native)}, true, MODULE_ATTRIBUTE_DEFAULT}

#define MODULE_EXPORT_TYPE(type)                                                \
    {nullptr, {.obj=(ArObject *) (type)}, false, MODULE_ATTRIBUTE_DEFAULT}

#define MODULE_EXPORT_TYPE_ALIAS(name, type)                                    \
    {name, {.obj=(ArObject *) (type)}, false, MODULE_ATTRIBUTE_DEFAULT}

#define ARGON_MODULE_SENTINEL {nullptr, nullptr, false, (argon::vm::datatype::AttributeFlag) 0}

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

    inline void ModuleSetDLHandle(Module *mod, ModuleFiniFn cleanup, uintptr_t opaque){
        mod->_nfini = cleanup;
        mod->_dlhandle = opaque;
    }

} // namespace argon::vm::datatype

#endif // !ARGON_DATATYPE_MODULE_H_
