// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"
#include "tuple.h"
#include "list.h"

using namespace argon::vm::datatype;

ArObject *tuple_iter(Tuple *self, bool reverse) {
    auto *li = MakeGCObject<TupleIterator>(type_tuple_iterator_);

    if (li != nullptr) {
        li->tuple = IncRef(self);
        li->index = 0;
        li->reverse = reverse;
    }

    return (ArObject *) li;
}

TypeInfo TupleType = {
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
        (UnaryBoolOp) tuple_iter,
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

// TUPLE ITERATOR

ArObject *tupleiterator_iter(TupleIterator *self, bool reversed) {
    if (self->reverse == reversed)
        return (ArObject *) IncRef(self);

    return tuple_iter(self->tuple, reversed);
}

ArObject *tupleiterator_iter_next(TupleIterator *self) {
    if (!self->reverse) {
        if (self->index < self->tuple->length) {
            auto *tmp = IncRef(self->tuple->objects[self->index]);

            self->index++;

            return tmp;
        }
        return nullptr;
    }

    if (self->tuple->length - self->index == 0)
        return nullptr;

    self->index++;

    return IncRef(self->tuple->objects[self->tuple->length - self->index]);
}

TypeInfo TupleIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "TupleIterator",
        nullptr,
        nullptr,
        sizeof(List),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (UnaryBoolOp) tupleiterator_iter,
        (UnaryOp) tupleiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_tuple_iterator_ = &TupleIteratorType;