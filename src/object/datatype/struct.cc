// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "error.h"

#include "struct.h"

using namespace argon::object;
using namespace argon::memory;

void struct_cleanup(Struct *self) {
    Release(self->name);
    Release(self->names);
    Release(self->impls);
}

ArObject *struct_get_static_attr(Struct *self, ArObject *key) {
    PropertyInfo pinfo{};
    ArObject *obj;

    obj = NamespaceGetValue(self->names, key, &pinfo);
    if (obj == nullptr) {
        ErrorFormat(&error_scope_error, "unknown attribute '%s' of object '%s'", ((String *) key)->buffer,
                    ((String *) self->name)->buffer);
        Release(obj);
        return nullptr;
    }

    if (!pinfo.IsStatic()) {
        ErrorFormat(&error_access_violation,
                    "in order to access to non const member '%s' an instance of '%s' is required",
                    ((String *) key)->buffer,
                    ((String *) self->name)->buffer);
        Release(obj);
        return nullptr;
    }

    if (!pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer,
                    ((String *) self->name)->buffer);
        Release(obj);
        return nullptr;
    }

    return obj;
}

const ObjectSlots struct_actions{
        nullptr,
        nullptr,
        (BinaryOp) struct_get_static_attr,
        nullptr,
        nullptr
};

const TypeInfo argon::object::type_struct_ = {
        TYPEINFO_STATIC_INIT,
        "struct",
        nullptr,
        sizeof(Struct),
        nullptr,
        (VoidUnaryOp) struct_cleanup,
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
        nullptr,
        &struct_actions,
        nullptr,
        nullptr
};

Struct *argon::object::StructNew(String *name, Namespace *names, List *mro) {
    auto stru = ArObjectNew<Struct>(RCType::INLINE, &type_struct_);

    if (stru != nullptr) {
        IncRef(name);
        stru->name = name;
        IncRef(names);
        stru->names = names;
        IncRef(mro);
        stru->impls = mro;

        // Looking into 'names' and counts the number of instantiable properties.
        stru->properties_count = 0;
        for (auto *cur = (NsEntry *) names->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
            if (!cur->info.IsStatic() && !cur->info.IsConstant()) stru->properties_count++;
        }
    }

    return stru;
}
