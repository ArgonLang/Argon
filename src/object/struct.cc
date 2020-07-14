// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "struct.h"

using namespace argon::object;
using namespace argon::memory;

void struct_cleanup(Struct *self) {
    Release(self->name);
    Release(self->names);
    Release(self->impls);
}

const TypeInfo argon::object::type_struct_ = {
        (const unsigned char *) "struct",
        sizeof(Struct),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (VoidUnaryOp) struct_cleanup
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
        for (NsEntry *cur = names->iter_begin; cur != nullptr; cur = cur->iter_next) {
            if (cur->info.IsMember() && !cur->info.IsConstant()) stru->properties_count++;
        }
    }

    return stru;
}
