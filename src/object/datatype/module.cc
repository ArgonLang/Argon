// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <string>

#include "nil.h"
#include "function.h"
#include "module.h"

using namespace argon::object;
using namespace argon::memory;

const TypeInfo type_module_ = {
        (const unsigned char *) "module",
        sizeof(Module),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool InsertID(Module *module, const std::string &id, ArObject *value) {
    ArObject *key = StringIntern(id);

    if (key == nullptr)
        return false;

    bool ok = NamespaceNewSymbol(module->module_ns, PropertyInfo(PropertyType::CONST), key, value);

    Release(key);
    return ok;
}

bool InitGlobals(Module *module) {
    if ((module->module_ns = NamespaceNew()) != nullptr) {
        if (!InsertID(module, "__name", module->name))
            return false;

        if (!InsertID(module, "__doc", module->doc))
            return false;

        return true;
    }

    return false;
}

Module *argon::object::ModuleNew(const std::string &name, const std::string &doc) {
    auto module = ArObjectNew<Module>(RCType::INLINE, &type_module_);

    if (module != nullptr) {
        if ((module->name = StringNew(name)) == nullptr)
            goto error;

        if ((module->doc = StringNew(doc)) == nullptr)
            goto error;

        // Initialize module globals
        if (!InitGlobals(module))
            goto error;

        return module;
    }

    error:
    Release(module);
    return nullptr;
}

Module *argon::object::ModuleNew(const ModuleInit *init) {
    auto module = ArObjectNew<Module>(RCType::INLINE, &type_module_);

    if (module != nullptr) {
        if ((module->name = StringNew(init->name)) == nullptr)
            goto error;

        module->doc = nullptr;
        if (init->doc != nullptr) {
            if ((module->doc = StringNew(init->doc)) == nullptr)
                goto error;
        }

        // Initialize module globals
        if (!InitGlobals(module))
            goto error;

        if (!ModuleAddObjects(module, init->bulk))
            goto error;

        return module;
    }

    error:
    Release(module);
    return nullptr;
}

bool argon::object::ModuleAddObjects(Module *module, const PropertyBulk *bulk) {
    ArObject *key;
    bool ok = true;

    for (const PropertyBulk *cursor = bulk; cursor->name != nullptr && ok; cursor++) {
        ArObject *obj = cursor->prop.obj;

        if (cursor->is_func) {
            if ((obj = FunctionNew(module->module_ns, cursor->prop.func)) == nullptr) {
                ok = false;
                break;
            }
        }

        if ((key = StringNew(cursor->name)) == nullptr)
            return false;

        // Object
        ok = NamespaceNewSymbol(module->module_ns, cursor->info, key, obj);
        Release(key);
        if (cursor->is_func)
            Release(obj);
    }

    return ok;
}