// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "instance.h"

using namespace argon::object;
using namespace argon::memory;

void instance_cleanup(Instance *self) {
    Release(self->base);
    Release(self->properties);
}

const TypeInfo argon::object::type_instance_ = {
        (const unsigned char *) "instance",
        sizeof(Instance),
        nullptr,
        nullptr,
        nullptr,
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