// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"
#include "tuple.h"
#include "list.h"

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

ArObject *argon::vm::datatype::TupleGet(const Tuple *tuple, ArSSize index) {
    if (index < 0)
        index = (ArSSize) tuple->length + index;

    if (index >= 0 && index < tuple->length)
        return IncRef(tuple->objects[index]);

    // TODO: SetError?!

    return nullptr;
}

bool argon::vm::datatype::TupleInsert(Tuple *tuple, ArObject *object, ArSize index) {
    if (index >= tuple->length)
        return false;

    Release(tuple->objects[index]);

    tuple->objects[index] = object == nullptr ? (ArObject *) Nil : IncRef(object);

    return true;
}

Tuple *argon::vm::datatype::TupleNew(ArObject *iterable) {
    auto *tuple = MakeObject<Tuple>(type_tuple_);

    if (tuple == nullptr)
        return nullptr;

    tuple->objects = nullptr;
    tuple->length = 0;

    if (AR_TYPEOF(iterable, type_list_)) {
        // Fast-path
        const auto *list = (List *) iterable;

        tuple->length = list->length;

        if (list->length > 0) {
            tuple->objects = (ArObject **) argon::vm::memory::Alloc(list->length * sizeof(void *));
            if (tuple->objects == nullptr) {
                Release(tuple);
                return nullptr;
            }

            for (ArSize i = 0; i < tuple->length; i++)
                tuple->objects[i] = list->objects[i];
        }

        return tuple;
    }

    assert(false);

    return tuple;
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