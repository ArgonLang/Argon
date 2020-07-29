// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "instance.h"
#include "trait.h"
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
        if (obj == nullptr || (pinfo.IsConstant() && !pinfo.IsMember())) {
            Release(obj);

            // Search in parent MRO!
            for (size_t i = 0; i < self->base->impls->len; i++) {
                auto trait = (Trait *) self->base->impls->objects[i];
                obj = NamespaceGetValue(trait->names, key, &pinfo);
                if (obj != nullptr && pinfo.IsMember())
                    break;
            }

            assert(obj != nullptr);
        }
    }

    if (obj->type == &type_function_ && (pinfo.IsConstant() && pinfo.IsMember())) {
        auto tmp = FunctionNew((Function *) obj, self);
        Release(obj);
        obj = tmp;
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
        nullptr,
        (VoidUnaryOp) instance_cleanup
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