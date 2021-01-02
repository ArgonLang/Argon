// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>
#include <memory/memory.h>

#include "bounds.h"
#include "error.h"
#include "list.h"
#include "nil.h"
#include "string.h"
#include "tuple.h"

using namespace argon::object;

size_t tuple_len(Tuple *self) {
    return self->len;
}

ArObject *argon::object::TupleGetItem(Tuple *self, ArSSize i) {
    ArObject *obj;

    if (i < self->len) {
        obj = self->objects[i];
        IncRef(obj);
        return obj;
    }

    return ErrorFormat(&error_overflow_error, "tuple index out of range (len: %d, idx: %d)", self->len, index);
}

ArObject *tuple_get_slice(Tuple *self, Bounds *bounds) {
    ArObject *tmp;
    Tuple *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, self->len, &start, &stop, &step);

    if ((ret = TupleNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (size_t i = 0; start < stop; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    } else {
        for (size_t i = 0; stop < start; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    }

    return ret;
}

const SequenceSlots tuple_sequence{
        (SizeTUnaryOp) tuple_len,
        (BinaryOpArSize) argon::object::TupleGetItem,
        nullptr,
        (BinaryOp) tuple_get_slice,
        nullptr
};

bool tuple_is_true(Tuple *self) {
    return self->len > 0;
}

bool tuple_equal(Tuple *self, ArObject *other) {
    auto *o = (Tuple *) other;

    if (self != other) {
        if (!AR_SAME_TYPE(self, other) || self->len != o->len)
            return false;

        for (size_t i = 0; i < self->len; i++) {
            if (!AR_EQUAL(self->objects[i], o->objects[i]))
                return false;
        }
    }

    return true;
}

size_t tuple_hash(Tuple *self) {
    unsigned long result = 1;
    ArObject *obj;
    size_t hash;

    if (self->len == 0)
        return 0;

    for (size_t i = 0; i < self->len; i++) {
        obj = self->objects[i];

        if ((hash = Hash(obj)) == 0 && argon::vm::IsPanicking())
            return 0;

        result = 31 * result + hash;
    }

    return result;
}

ArObject *tuple_str(Tuple *self) {
    return nullptr;
}

void tuple_cleanup(Tuple *self) {
    for (size_t i = 0; i < self->len; i++)
        Release(self->objects[i]);

    argon::memory::Free(self->objects);
}

const TypeInfo argon::object::type_tuple_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "tuple",
        sizeof(Tuple),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &tuple_sequence,
        (BoolUnaryOp) tuple_is_true,
        (BoolBinOp) tuple_equal,
        nullptr,
        (SizeTUnaryOp) tuple_hash,
        (UnaryOp) tuple_str,
        nullptr,
        nullptr,
        (VoidUnaryOp) tuple_cleanup
};

Tuple *argon::object::TupleNew(size_t len) {
    auto tuple = ArObjectNew<Tuple>(RCType::INLINE, &type_tuple_);

    if (tuple != nullptr) {
        tuple->len = len;

        if ((tuple->objects = (ArObject **) argon::memory::Alloc(len * sizeof(void *))) == nullptr) {
            Release(tuple);
            return nullptr;
        }

        for (size_t i = 0; i < len; i++) {
            IncRef(NilVal);
            tuple->objects[i] = NilVal;
        }
    }

    return tuple;
}

Tuple *argon::object::TupleNew(const ArObject *sequence) {
    Tuple *tuple = nullptr;
    ArObject *tmp;

    if (IsSequence(sequence)) {
        if ((tuple = ArObjectNew<Tuple>(RCType::INLINE, &type_tuple_)) == nullptr)
            return nullptr;

        if (AR_TYPEOF(sequence, type_list_)) {
            // List FAST-PATH
            auto list = (List *) sequence;

            if (list->len > 0) {
                tuple->objects = (ArObject **) argon::memory::Alloc(list->len * sizeof(void *));

                if (tuple->objects == nullptr) {
                    Release(tuple);
                    return nullptr;
                }

                auto other = (const ArObject **) list->objects;
                for (size_t i = 0; i < list->len; i++) {
                    tmp = (ArObject *) other[i];
                    IncRef(tmp);
                    tuple->objects[i] = tmp;
                }

                tuple->len = list->len;
            }

            return tuple;
        }
    }

    Release((ArObject **) &tuple);
    ErrorFormat(&error_not_implemented, "no viable conversion from '%s' to tuple", AR_TYPE_NAME(sequence));
    return nullptr;
}

Tuple *argon::object::TupleNew(ArObject *result, ArObject *error) {
    Tuple *tuple;

    if ((tuple = TupleNew(2)) != nullptr) {
        if (result == nullptr) {
            IncRef(NilVal);
            result = NilVal;
        }

        if (error == nullptr) {
            IncRef(NilVal);
            error = NilVal;
        }

        TupleInsertAt(tuple, 0, result);
        TupleInsertAt(tuple, 1, error);
        return tuple;
    }

    return nullptr;
}

bool argon::object::TupleInsertAt(Tuple *tuple, size_t idx, ArObject *obj) {
    if (idx >= tuple->len)
        return false;

    Release(tuple->objects[idx]);

    IncRef(obj);
    tuple->objects[idx] = obj;
    return true;
}