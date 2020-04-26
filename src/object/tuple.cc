// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "tuple.h"
#include "list.h"

#include <memory/memory.h>

using namespace argon::object;
using namespace argon::memory;

bool tuple_equal(ArObject *self, ArObject *other) {
    return false;
}

size_t tuple_hash(ArObject *obj) {
    return 0;
}

void tuple_cleanup(ArObject *obj) {
    auto tuple = (Tuple *) obj;
    for (size_t i = 0; i < tuple->len; i++)
        Release(tuple->objects[i]);
}

size_t tuple_len(ArObject *obj) {
    return ((Tuple *) obj)->len;
}

ArObject *argon::object::TupleGetItem(Tuple *tuple, size_t i) {
    ArObject *obj = nullptr;
    if (i >= tuple->len) {
        assert(i >= tuple->len);
        return nullptr;
    }
    obj = tuple->objects[i];
    IncRef(obj);
    return obj;
}

const SequenceActions tuple_actions{
        tuple_len,
        (BinaryOpSizeT) argon::object::TupleGetItem
};

const TypeInfo type_tuple_ = {
        (const unsigned char *) "tuple",
        sizeof(Tuple),
        nullptr,
        &tuple_actions,
        nullptr,
        nullptr,
        tuple_equal,
        nullptr,
        tuple_hash,
        nullptr,
        tuple_cleanup
};

Tuple *argon::object::TupleNew(const ArObject *sequence) {
    auto tuple = (Tuple *) argon::memory::Alloc(sizeof(Tuple));
    ArObject *tmp;

    assert(tuple != nullptr);

    tuple->strong_or_ref = 1;
    tuple->type = &type_tuple_;

    if (IsSequence(sequence)) {
        if (sequence->type == &type_list_) {
            // List FAST-PATH
            auto list = (List *) sequence;
            if (list->len > 0) {
                tuple->objects = (ArObject **) Alloc(list->len * sizeof(ArObject *));
                assert(tuple->objects != nullptr);
                tuple->len = list->len;

                auto other_buf = (const ArObject **) list->objects;
                for (size_t i = 0; i < list->len; i++) {
                    tmp = (ArObject *) other_buf[i];
                    IncRef(tmp);
                    tuple->objects[i] = tmp;
                }
            }
            return tuple;
        }

        // TODO: implement generics
        assert(false);
    }

    return tuple;
}