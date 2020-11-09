// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"
#include "function.h"
#include "error.h"

#include "module.h"

using namespace argon::object;
using namespace argon::memory;

ArObject *module_get_static_attr(Module *self, ArObject *key) {
    PropertyInfo pinfo{};
    ArObject *obj;

    if ((obj = NamespaceGetValue(self->module_ns, key, &pinfo)) == nullptr) {
        ErrorFormat(&error_attribute_error, "unknown attribute '%s' of module '%s'", ((String *) key)->buffer,
                    self->name->buffer);
        return nullptr;
    }

    if (!pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of module '%s' are private",
                    ((String *) key)->buffer,
                    ((String *) self->name)->buffer);
        Release(obj);
        return nullptr;
    }

    return obj;
}

bool module_set_static_attr(Module *self, ArObject *key, ArObject *value) {
    PropertyInfo pinfo{};

    if (!NamespaceContains(self->module_ns, key, &pinfo)) {
        ErrorFormat(&error_attribute_error, "unknown attribute '%s' of module '%s'", ((String *) key)->buffer,
                    self->name->buffer);
        return false;
    }

    if (!pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of module '%s' are private",
                    ((String *) key)->buffer, ((String *) self->name)->buffer);
        return false;
    }


    if (pinfo.IsConstant()) {
        ErrorFormat(&error_unassignable_variable, "unable to assign value to constant '%s::%s'",
                    ((String *) self->name)->buffer, ((String *) key)->buffer);
        return false;
    }

    NamespaceSetValue(self->module_ns, key, value);

    return true;
}

const ObjectActions module_actions = {
        nullptr,
        (BinaryOp) module_get_static_attr,
        nullptr,
        (BoolTernOp) module_set_static_attr
};

const TypeInfo type_module_ = {
        (const unsigned char *) "module",
        sizeof(Module),
        nullptr,
        nullptr,
        nullptr,
        &module_actions,
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

Module *argon::object::ModuleNew(String *name, String *doc) {
    auto module = ArObjectNew<Module>(RCType::INLINE, &type_module_);

    if (module != nullptr) {
        IncRef(name);
        module->name = name;
        IncRef(doc);
        module->doc = doc;

        // Initialize module globals
        if (!InitGlobals(module)) {
            Release(module);
            return nullptr;
        }
    }

    return module;
}

Module *argon::object::ModuleNew(const char *name, const char *doc) {
    Module *module;
    String *arname;
    String *ardoc;

    if ((arname = StringNew(name)) == nullptr)
        return nullptr;

    if ((ardoc = StringNew(doc)) == nullptr) {
        Release(arname);
        return nullptr;
    }

    module = ModuleNew(arname, ardoc);
    Release(arname);
    Release(ardoc);

    return module;
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