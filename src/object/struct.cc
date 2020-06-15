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
        (VoidUnaryOp) struct_cleanup
};

Struct * argon::object::StructNew(String *name, Namespace *names, List *mro) {
    auto stru = (Struct *) Alloc(sizeof(Struct));

    if (stru != nullptr) {
        stru->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
        stru->type = &type_struct_;

        IncRef(name);
        stru->name = name;
        IncRef(names);
        stru->names = names;
        IncRef(mro);
        stru->impls = mro;
    }

    return stru;
}
