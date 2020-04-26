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
        nullptr
};

bool InsertID(Module *module, const std::string &id, ArObject *value) {
    ArObject *key = StringIntern(id);

    if (key == nullptr)
        return false;

    bool ok = MapInsert(module->module_ns, key, value);

    Release(key);
    return ok;
}

bool InitGlobals(Module *module) {
    if ((module->module_ns = MapNew()) != nullptr) {
        if (!InsertID(module, "__name", module->name))
            return false;

        return true;
    }

    return false;
}

Module *argon::object::ModuleNew(const std::string &name) {
    auto module = (Module *) Alloc(sizeof(Module));

    if (module != nullptr) {
        module->strong_or_ref = 1;
        module->type = &type_module_;

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