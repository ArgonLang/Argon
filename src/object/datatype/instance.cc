// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "instance.h"
#include "function.h"

using namespace argon::object;
using namespace argon::memory;

void instance_cleanup(Instance *self) {
    Release(self->base);
    Release(self->properties);
}

ArObject *instance_getattr(Instance *self, ArObject *key) {
    PropertyInfo pinfo{};
    ArObject *obj;

    obj = NamespaceGetValue(self->properties, key, &pinfo);

    if (obj == nullptr) {
        // Search in parent
        obj = NamespaceGetValue(self->base->names, key, &pinfo);
        if (obj == nullptr || (pinfo.IsConstant() && pinfo.IsStatic())) {
            Release(obj);

            // Search in parent MRO!
            if (self->base->impls != nullptr) {
                for (size_t i = 0; i < self->base->impls->len; i++) {
                    auto trait = (Trait *) self->base->impls->objects[i];
                    obj = NamespaceGetValue(trait->names, key, &pinfo);
                    if (obj != nullptr && !pinfo.IsStatic())
                        break;
                }
            }
        }
    }

    if (obj == nullptr) {
        ErrorFormat(&error_attribute_error, "unknown attribute '%s' of object '%s'", ((String *) key)->buffer,
                    ((String *) self->base->name)->buffer);
        return nullptr;
    }

    if (argon::vm::GetRoutine()->frame->instance != self && !pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer,
                    ((String *) self->base->name)->buffer);
        Release(obj);
        return nullptr;
    }

    return obj;
}

bool instance_setattr(Instance *self, ArObject *key, ArObject *value) {
    PropertyInfo pinfo{};

    if (!NamespaceContains(self->properties, key, &pinfo)) {
        ErrorFormat(&error_attribute_error, "unknown attribute '%s' of object '%s'", ((String *) key)->buffer,
                    ((String *) self->base->name)->buffer);
        return false;
    }

    if (argon::vm::GetRoutine()->frame->instance != self && !pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer,
                    ((String *) self->base->name)->buffer);
        return false;
    }

    return NamespaceSetValue(self->properties, key, value);
}

const ObjectSlots instance_actions{
        nullptr,
        (BinaryOp) instance_getattr,
        nullptr,
        (BoolTernOp) instance_setattr,
        nullptr
};

const TypeInfo argon::object::type_instance_ = {
        TYPEINFO_STATIC_INIT,
        "instance",
        nullptr,
        sizeof(Instance),
        nullptr,
        (VoidUnaryOp) instance_cleanup,
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
        nullptr,
        &instance_actions,
        nullptr,
        nullptr
};

Instance *argon::object::InstanceNew(Struct *base, Namespace *properties) {
    auto instance = ArObjectNew<Instance>(RCType::INLINE, &type_instance_);

    if (instance != nullptr) {
        IncRef(base);
        instance->base = base;
        IncRef(properties);
        instance->properties = properties;
    }

    return instance;
}