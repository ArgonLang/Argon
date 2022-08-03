// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"
#include "tuple.h"

using namespace argon::vm::datatype;

const TypeInfo TupleType = {
        AROBJ_HEAD_INIT_TYPE,
        "Tuple",
        nullptr,
        nullptr,
        sizeof(Tuple),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_tuple_ = &TupleType;

bool argon::vm::datatype::TupleInsert(Tuple *tuple, ArObject *object, ArSize index) {
    if (index >= tuple->length)
        return false;

    Release(tuple->objects[index]);

    tuple->objects[index] = object == nullptr ? (ArObject *) Nil : IncRef(object);

    return true;
}

Tuple *argon::vm::datatype::TupleNew(ArSize length) {
    auto *tuple = MakeObject<Tuple>(type_tuple_);

    if (tuple != nullptr) {
        tuple->objects = nullptr;
        tuple->length = length;

        if (length > 0) {
            tuple->objects = (ArObject **) argon::vm::memory::Alloc(length * sizeof(void *));
            if (tuple->objects == nullptr) {
                Release(tuple);
                return nullptr;
            }

            for (ArSize i = 0; i < length; i++)
                tuple->objects[i] = (ArObject *) Nil;
        }
    }

    return tuple;
}

Tuple *argon::vm::datatype::TupleNew(ArObject **objects, ArSize count) {
    auto *tuple = TupleNew(count);

    if (tuple != nullptr) {
        for (ArSize i = 0; i < count; i++)
            tuple->objects[i] = IncRef(objects[i]);
    }

    return tuple;
}