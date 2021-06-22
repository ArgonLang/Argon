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
            return ErrorFormat(type_attribute_error_, "unknown attribute '%s' of instance '%s'",
                               ((String *) key)->buffer, ancestor->name);
    }

    if (!pinfo.IsPublic() && !TraitIsImplemented(instance, ancestor)) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of '%s' are private",
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
        ErrorFormat(type_attribute_error_, "unknown attribute '%s' of instance '%s'", ((String *) key)->buffer,
                    AR_TYPE_NAME(self));
        return false;
    }

    if (!pinfo.IsPublic() && instance != self) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, AR_TYPE_NAME(self));
        return false;
    }

    return NamespaceSetValue(self->names, key, value);
}

const ObjectSlots struct_actions{
        nullptr,
        nullptr,
        (BinaryOp) struct_get_attr,
        (BinaryOp) struct_get_static_attr,
        (BoolTernOp) struct_set_attr,
        nullptr,
        -1
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
    auto *instance = (Struct *) ArObjectGCNew(type);

    if (instance != nullptr) {
        instance->names = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST);
        if (instance->names == nullptr) {
            Release(instance);
            return nullptr;
        }

        if (NamespaceSetPositional(instance->names, values, count) >= 1) {
            Release(instance);
            return (Struct *) ErrorFormat(type_undeclared_error_, "too many args to initialize struct '%s'",
                                          type->name);
        }
    }

    return instance;
}

Struct *argon::object::StructNewKeyPair(TypeInfo *type, ArObject **values, ArSize count) {
    auto *instance = (Struct *) ArObjectGCNew(type);

    if (instance != nullptr) {
        instance->names = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST);
        if (instance->names == nullptr) {
            Release(instance);
            return nullptr;
        }

        for (ArSize i = 0; i < count; i += 2) {
            if (!NamespaceSetValue(instance->names, values[i], values[i + 1])) {
                Release(instance);
                return (Struct *) ErrorFormat(type_undeclared_error_, "struct '%s' have no property named '%s'",
                                              type->name, ((String *) values[i])->buffer);
            }
        }
    }

    return instance;
}

