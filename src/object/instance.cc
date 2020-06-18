// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "instance.h"
#include "trait.h"

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
        if (obj == nullptr || pinfo.IsConstant()) {
            Release(obj);

            // Search in parent MRO!
            for (size_t i = 0; self->base->impls->len; i++) {
                auto trait = (Trait *) self->base->impls->objects[i];
                obj = NamespaceGetValue(trait->names, key, &pinfo);
                if (obj != nullptr && !pinfo.IsConstant())
                    break;
            }

            assert(obj != nullptr);
        }
    }

    return obj; // TODO impl error: value nopt found / priv variable
}

bool instance_setattr(Instance *self, ArObject *key, ArObject *value) {
    // TODO: check access permission!
    return NamespaceSetValue(self->properties, key, value); // TODO: invalid key!
}

const ObjectActions instance_actions{
        (BinaryOp) instance_getattr,
        (BoolTernOp) instance_setattr
};

const TypeInfo argon::object::type_instance_ = {
        (const unsigned char *) "instance",
        sizeof(Instance),
        nullptr,
        nullptr,
        nullptr,
        &instance_actions,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (VoidUnaryOp) instance_cleanup
};

Instance *argon::object::InstanceNew(Struct *base, Namespace *properties) {
    auto instance = (Instance *) Alloc(sizeof(Instance));

    if (instance != nullptr) {
        instance->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
        instance->type = &type_instance_;

        IncRef(base);
        instance->base = base;
        IncRef(properties);
        instance->properties = properties;
    }

    return instance;
}