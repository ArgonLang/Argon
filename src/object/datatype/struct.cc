// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "tuple.h"
#include "struct.h"

using namespace argon::object;
using namespace argon::memory;

const ObjectSlots struct_actions{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        offsetof(Struct, names)
};

ArObject *struct_str(Struct *self) {
    ArObject *args[] = {self};
    ArObject *func;
    ArObject *ret;
    String *key;

    if ((key = StringIntern("__str")) == nullptr)
        return nullptr;

    func = PropertyGet(self, key, true);
    Release(key);

    if (func != nullptr) {
        ret = argon::vm::Call(func, 1, args);
        Release(func);
        return ret;
    }

    Release(argon::vm::GetLastError()); // Ignore undeclared_method or private_modifier
    return StringNewFormat("%s object at %p", AR_TYPE_NAME(self), self);
}

void struct_cleanup(Struct *self) {
    Release(self->names);
}

void struct_trace(Struct *self, VoidUnaryOp trace) {
    trace(self->names);
}

const TypeInfo StructType = {
        TYPEINFO_STATIC_INIT,
        "struct",
        nullptr,
        sizeof(Struct),
        TypeInfoFlags::STRUCT,
        nullptr,
        (VoidUnaryOp) struct_cleanup,
        (Trace) struct_trace,
        nullptr,
        nullptr,
        nullptr,
        (UnaryOp) struct_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &struct_actions,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_struct_ = &StructType;

Struct *argon::object::StructNewPositional(TypeInfo *type, ArObject **values, ArSize count) {
    auto *instance = ArObjectGCNew(type);
    Namespace **ns;

    if (instance == nullptr)
        return nullptr;

    if (type->obj_actions->nsoffset == -1) {
        // fill real struct!
        return (Struct *) instance;
    }

    // Initialize new namespace
    ns = (Namespace **) AR_GET_NSOFF(instance);

    if ((*ns = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST)) == nullptr) {
        Release(instance);
        return nullptr;
    }

    if (NamespaceSetPositional(*ns, values, count) >= 1) {
        Release(instance);
        return (Struct *) ErrorFormat(type_undeclared_error_, "too many args to initialize struct '%s'",
                                      type->name);
    }

    return (Struct *) instance;
}

Struct *argon::object::StructNewKeyPair(TypeInfo *type, ArObject **values, ArSize count) {
    auto *instance = ArObjectGCNew(type);
    Namespace **ns;

    if (instance == nullptr)
        return nullptr;

    if (type->obj_actions->nsoffset == -1) {
        // fill real struct!
        return (Struct *) instance;
    }

    // Initialize new namespace
    ns = (Namespace **) AR_GET_NSOFF(instance);

    if ((*ns = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST)) == nullptr) {
        Release(instance);
        return nullptr;
    }

    for (ArSize i = 0; i < count; i += 2) {
        if (!NamespaceSetValue(*ns, values[i], values[i + 1])) {
            Release(instance);
            return (Struct *) ErrorFormat(type_undeclared_error_, "struct '%s' have no property named '%s'",
                                          type->name, ((String *) values[i])->buffer);
        }
    }

    return (Struct *) instance;
}

