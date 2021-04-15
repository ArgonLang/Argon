// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "tuple.h"
#include "struct.h"

using namespace argon::object;
using namespace argon::memory;

ArObject *struct_get_attr(Struct *self, ArObject *key) {
    PropertyInfo pinfo{};

    const TypeInfo *ancestor = AR_GET_TYPE(self);
    const TypeInfo *type;

    ArObject *instance = nullptr;
    ArObject *obj;

    obj = NamespaceGetValue(self->names, key, &pinfo);

    if (argon::vm::GetRoutine()->frame != nullptr)
        instance = argon::vm::GetRoutine()->frame->instance;

    if (obj == nullptr) {

        if (ancestor->tp_map != nullptr)
            obj = NamespaceGetValue((Namespace *) ancestor->tp_map, key, &pinfo);

        if (obj == nullptr && ancestor->mro != nullptr) {
            for (ArSize i = 0; i < ((Tuple *) ancestor->mro)->len; i++) {
                type = (TypeInfo *) ((Tuple *) ancestor->mro)->objects[i];

                if (type->tp_map != nullptr) {
                    if ((obj = NamespaceGetValue((Namespace *) type->tp_map, key, &pinfo)) != nullptr)
                        break;
                }
            }
        }

        if (obj == nullptr)
            return ErrorFormat(&error_attribute_error, "unknown attribute '%s' of instance '%s'",
                               ((String *) key)->buffer, ancestor->name);
    }

    if (!pinfo.IsPublic() && !TraitIsImplemented(instance, ancestor)) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, ancestor->name);
        Release(obj);
        return nullptr;
    }

    return obj;
}

ArObject *struct_get_static_attr(Struct *self, ArObject *key) {
    const TypeInfo *ancestor = AR_GET_TYPE(self)->type;

    return ancestor->obj_actions->get_static_attr((ArObject *) AR_GET_TYPE(self), key);
}

bool struct_set_attr(Struct *self, ArObject *key, ArObject *value) {
    PropertyInfo pinfo{};
    ArObject *instance = nullptr;

    if (argon::vm::GetRoutine()->frame != nullptr)
        instance = argon::vm::GetRoutine()->frame->instance;

    if (!NamespaceContains(self->names, key, &pinfo)) {
        ErrorFormat(&error_attribute_error, "unknown attribute '%s' of instance '%s'", ((String *) key)->buffer,
                    AR_TYPE_NAME(self));
        return false;
    }

    if (!pinfo.IsPublic() && instance != self) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, AR_TYPE_NAME(self));
        return false;
    }

    return NamespaceSetValue(self->names, key, value);
}

const ObjectSlots struct_actions{
        nullptr,
        (BinaryOp) struct_get_attr,
        (BinaryOp) struct_get_static_attr,
        (BoolTernOp) struct_set_attr,
        nullptr
};

void struct_cleanup(Struct *self) {
    Release(self->names);
}

void struct_trace(Struct *self, VoidUnaryOp trace) {
    trace(self->names);
}

const TypeInfo argon::object::type_struct_ = {
        TYPEINFO_STATIC_INIT,
        "struct",
        nullptr,
        sizeof(Struct),
        nullptr,
        (VoidUnaryOp) struct_cleanup,
        (Trace) struct_trace,
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
        &struct_actions,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        TypeInfoFlags::STRUCT
};

Struct *argon::object::StructNew(TypeInfo *type, ArObject **values, ArSize count) {
    auto *instance = (Struct *) ArObjectGCNew(type);
    int res;

    if (instance != nullptr) {
        instance->names = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST | PropertyType::STATIC);
        if (instance->names == nullptr) {
            Release(instance);
            return nullptr;
        }

        res = NamespaceSetPositional(instance->names, values, count);
        // TODO: check for res >= 1
    }

    return instance;
}

