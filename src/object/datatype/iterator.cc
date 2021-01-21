// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "iterator.h"

using namespace argon::object;

Iterator *argon::object::IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed) {
    auto iter = ArObjectNew<Iterator>(RCType::INLINE, type);

    if (iter != nullptr) {
        iter->obj = IncRef(iterable);

        iter->index = 0;

        if (reversed) {
            if ((iter->index = Length(iterable)) < 0) {
                Release(iter);
                return nullptr;
            }
        }

        iter->reversed = reversed;
    }

    return iter;
}

void argon::object::IteratorReset(Iterator *iterator) {
    if (iterator->reversed) {
        iterator->index = Length(iterator->obj);
        return;
    }

    iterator->index = 0;
}