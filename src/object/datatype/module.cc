// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"
#include "error.h"
#include "function.h"
#include "integer.h"
#include "module.h"

using namespace argon::object;

ArObject *module_get_static_attr(Module *self, ArObject *key) {
    auto *skey = (String *) key;

    PropertyInfo info{};
    ArObject *obj;

    if (!AR_TYPEOF(key, type_string_)) {
        ErrorFormat(type_type_error_, "expected property name, found '%s'", AR_TYPE_NAME(key));
        return nullptr;
    }

    if ((obj = NamespaceGetValue(self->module_ns, key, &info)) == nullptr) {
        ErrorFormat(type_attribute_error_, "unknown attribute '%s' of module '%s'", skey->buffer, self->name->buffer);
        return nullptr;
    }

    if (!info.IsPublic()) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of module '%s' are private", skey->buffer,
                    self->name->buffer);
        Release(obj);
        return nullptr;
    }

    return obj;
}

bool module_set_static_attr(Module *self, ArObject *key, ArObject *value) {
    auto *skey = (String *) key;

    PropertyInfo pinfo{};

    if (!AR_TYPEOF(key, type_string_)) {
        ErrorFormat(type_type_error_, "expected property name, found '%s'", AR_TYPE_NAME(key));
        return false;
    }

    if (!NamespaceContains(self->module_ns, key, &pinfo)) {
        ErrorFormat(type_attribute_error_, "unknown attribute '%s' of module '%s'", skey->buffer, self->name->buffer);
        return false;
    }

    if (!pinfo.IsPublic()) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of module '%s' are private", skey->buffer,
                    self->name->buffer);
        return false;
    }

    if (pinfo.IsConstant()) {
        ErrorFormat(type_unassignable_error_, "unable to assign value to constant '%s::%s'", self->name->buffer,
                    skey->buffer);
        return false;
    }

    NamespaceSetValue(self->module_ns, key, value);

    return true;
}

const ObjectSlots module_oslots = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) module_get_static_attr,
        nullptr,
        (BoolTernOp) module_set_static_attr,
        -1
};

ArObject *module_str(Module *self) {
    return StringCFormat("<module '%s'>", self->name);
}

bool module_is_true(ArObject *self) {
    return true;
}

ArObject *module_compare(Module *self, ArObject *other, CompareMode mode) {
    auto *o = (Module *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other)
        return BoolToArBool(Equal(self->name, o->name) && Equal(self->doc, o->doc));

    return BoolToArBool(true);
}

void module_trace(Module *self, VoidUnaryOp trace) {
    trace(self->module_ns);
}

void module_cleanup(Module *self) {
    Release(self->module_ns);
}

const TypeInfo ModuleType = {
        TYPEINFO_STATIC_INIT,
        "module",
        nullptr,
        sizeof(Module),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) module_cleanup,
        (Trace) module_trace,
        (CompareOp) module_compare,
        module_is_true,
        nullptr,
        nullptr,
        (UnaryOp) module_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &module_oslots,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_module_ = &ModuleType;

bool InsertID(Module *module, const char *id, ArObject *value) {
    ArObject *key;
    bool ok = false;

    if ((key = StringIntern(id)) != nullptr) {
        ok = NamespaceNewSymbol(module->module_ns, key, value, MODULE_ATTRIBUTE_PUB_CONST);
        Release(key);
    }

    return ok;
}

bool InitGlobals(Module *module) {
#define ADD_ID(name, obj)               \
    if(!InsertID(module, name, (obj)))  \
        return false

    if ((module->module_ns = NamespaceNew()) != nullptr) {
        ADD_ID("__name", module->name);
        ADD_ID("__doc", module->doc);
        return true;
    }

    return false;
#undef ADD_ID
}

Module *argon::object::ModuleNew(String *name, String *doc) {
    auto module = ArObjectGCNewTrack<Module>(type_module_);

    if (module != nullptr) {
        module->name = IncRef(name);
        module->doc = IncRef(doc);

        // Initialize module globals
        if (!InitGlobals(module)) {
            Release(module);
            return nullptr;
        }

        module->finalize = nullptr;
    }

    return module;
}

Module *argon::object::ModuleNew(const char *name, const char *doc) {
    String *ardoc = nullptr;
    String *arname;

    Module *module;

    if ((arname = StringNew(name)) == nullptr)
        return nullptr;

    if (doc != nullptr) {
        if ((ardoc = StringNew(doc)) == nullptr) {
            Release(arname);
            return nullptr;
        }
    }

    module = ModuleNew(arname, ardoc);
    Release(arname);
    Release(ardoc);

    return module;
}

Module *argon::object::ModuleNew(const ModuleInit *init) {
    auto module = ModuleNew(init->name, init->doc);

    if (module != nullptr) {
        if (init->initialize != nullptr && !init->initialize(module))
            goto error;

        if (init->bulk != nullptr && !ModuleAddObjects(module, init->bulk))
            goto error;

        module->finalize = init->finalize;

        return module;
    }

    error:
    Release(module);
    return nullptr;
}

bool argon::object::ModuleAddIntConstant(Module *module, const char *key, ArSSize value) {
    ArObject *intval;
    bool ok;

    if ((intval = IntegerNew(value)) == nullptr)
        return false;

    ok = NamespaceNewSymbol(module->module_ns, key, intval, MODULE_ATTRIBUTE_PUB_CONST);

    Release(intval);

    return ok;
}

bool argon::object::ModuleAddObjects(Module *module, const PropertyBulk *bulk) {
    const char *name;
    ArObject *obj;
    ArObject *key;

    bool ok = true;

    for (const PropertyBulk *cursor = bulk; cursor->prop.obj != nullptr && ok; cursor++) {
        name = cursor->name;
        obj = cursor->prop.obj;

        if (cursor->is_func) {
            if ((obj = FunctionNew(module->module_ns, nullptr, cursor->prop.func, false)) == nullptr) {
                ok = false;
                break;
            }
        }

        if (name == nullptr && AR_TYPEOF(cursor->prop.obj, type_type_))
            name = ((const TypeInfo *) cursor->prop.obj)->name;

        assert(name != nullptr);

        if ((key = StringNew(name)) == nullptr) {
            if (cursor->is_func)
                Release(obj);

            return false;
        }

        // Object
        ok = NamespaceNewSymbol(module->module_ns, key, obj, cursor->info);
        Release(key);
        if (cursor->is_func)
            Release(obj);
    }

    return ok;
}
