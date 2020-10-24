// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MODULE_H_
#define ARGON_OBJECT_MODULE_H_

#include <object/arobject.h>
#include "namespace.h"
#include "string.h"

namespace argon::object {
    struct Module : ArObject {
        String *name;
        String *doc;

        Namespace *module_ns;
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
        PropertyBulk *bulk;
    };

    Module *ModuleNew(const std::string &name, const std::string &doc);

    Module *ModuleNew(const ModuleInit *init);

    bool ModuleAddObjects(Module *module, const PropertyBulk *bulk);

} // namespace argon::object

#endif // !ARGON_OBJECT_MODULE_H_
