// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <string>
#include "module.h"
#include "nil.h"

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

    bool ok = NamespaceNewSymbol(module->module_ns, PropertyInfo(0), key, value);

    Release(key);
    return ok;
}

bool InitGlobals(Module *module) {
    if ((module->module_ns = NamespaceNew()) != nullptr) {
        if (!InsertID(module, "__name", module->name))
            return false;

        return true;
    }

    return false;
}

Module *argon::object::ModuleNew(const std::string &name) {
    auto module = ArObjectNew<Module>(RCType::INLINE, &type_module_);

    if (module != nullptr) {

        if ((module->name = StringNew(name)) == nullptr) {
            Release(module);
            return nullptr;
        }

        // Initialize module globals
        if (!InitGlobals(module)) {
            Release(module);
            return nullptr;
        }
    }

    return module;
}