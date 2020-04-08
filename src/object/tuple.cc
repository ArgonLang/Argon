// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "tuple.h"
#include "list.h"

#include <memory/memory.h>

using namespace argon::object;
using namespace argon::memory;

Tuple::Tuple(const Object *seq) : Sequence(&type_tuple_) {
    const Object **other_buf = nullptr;
    Object *tmp = nullptr;

    if (seq->type == &type_list_) {
        auto list = ((Sequence *) seq);

        if (list->len > 0) {
            this->objects = (Object **) Alloc(list->len * sizeof(Object *));
            this->len = list->len;

            other_buf = (const Object **) list->objects;
            for (size_t i = 0; i < list->len; i++) {
                tmp = (Object *) other_buf[i];
                IncStrongRef(tmp);
                this->objects[i] = tmp;
            }
        }
        return;
    }

    assert(false);
}

Tuple::~Tuple() {
    for (int i = 0; i < this->len; i++)
        ReleaseObject(this->objects[i]);
    argon::memory::Free(this->objects);
}

bool Tuple::EqualTo(const Object *other) {
    return false;
}

size_t Tuple::Hash() {
    return 0;
}

Object *Tuple::GetItem(const Object *i) {
    return nullptr;
}

Object *Tuple::GetItem(size_t i) {
    return this->objects[i];
}
