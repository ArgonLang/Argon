// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MODULE_H_
#define ARGON_OBJECT_MODULE_H_

#include <object/arobject.h>
#include "namespace.h"
#include "string.h"

#define MODULE_BULK_EXPORT_TYPE(name, type) \
    {name, {.obj=(ArObject *) &type}, false, PropertyInfo(PropertyType::CONST | PropertyType::PUBLIC)}

#define MODULE_BULK_EXPORT_FUNCTION(fn_native)  \
    {fn_native.name, {.func=&fn_native}, true, PropertyInfo(PropertyType::CONST | PropertyType::PUBLIC)}

namespace argon::object {

    using ModuleInitializeFn = bool (*)(struct Module *module);

    using ModuleFinalizeFn = void (*)(struct Module *module);

    struct Module : ArObject {
        String *name;
        String *doc;

        Namespace *module_ns;

        ModuleFinalizeFn finalize;
    };

    struct PropertyBulk {
        const char *name;
        union {
            ArObject *obj;
            struct FunctionNative *func; // Forward declaration (see function.h)
        } prop;
        bool is_func;
        PropertyInfo info;
    };

    struct ModuleInit {
        const char *name;
        const char *doc;

        const PropertyBulk *bulk;

        ModuleInitializeFn initialize;
        ModuleFinalizeFn finalize;
    };

    extern const TypeInfo type_module_;

    Module *ModuleNew(String *name, String *doc);

    Module *ModuleNew(const char *name, const char *doc);

    Module *ModuleNew(const ModuleInit *init);

    bool ModuleAddObjects(Module *module, const PropertyBulk *bulk);

    inline bool ModuleAddProperty(Module *module, ArObject *key, ArObject *value, PropertyInfo info) {
        return NamespaceNewSymbol(module->module_ns, info, key, value);
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_MODULE_H_
