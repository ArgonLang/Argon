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

ArObject *argon::object::TupleGetItem(Tuple *self, ArSSize index) {
    ArObject *obj;

    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        obj = self->objects[index];
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

ArObject *tuple_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("tuple", count, 0, 1))
        return nullptr;

    if (count == 1)
        return TupleNew(*args);

    return TupleNew((size_t) 0);
}

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
    StringBuilder sb = {};
    String *tmp = nullptr;

    if (StringBuilderWrite(&sb, (unsigned char *) "(", 1, self->len == 0 ? 1 : 0) < 0)
        goto error;

    for (size_t i = 0; i < self->len; i++) {
        if ((tmp = (String *) ToString(self->objects[i])) == nullptr)
            goto error;

        if (StringBuilderWrite(&sb, tmp, i + 1 < self->len ? 2 : 1) < 0)
            goto error;

        if (i + 1 < self->len) {
            if (StringBuilderWrite(&sb, (unsigned char *) ", ", 2) < 0)
                goto error;
        }

        Release(tmp);
    }

    if (StringBuilderWrite(&sb, (unsigned char *) ")", 1) < 0)
        goto error;

    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    return nullptr;
}

void tuple_cleanup(Tuple *self) {
    for (size_t i = 0; i < self->len; i++)
        Release(self->objects[i]);

    argon::memory::Free(self->objects);
}

const TypeInfo argon::object::type_tuple_ = {
        TYPEINFO_STATIC_INIT,
        "tuple",
        nullptr,
        sizeof(Tuple),
        tuple_ctor,
        (VoidUnaryOp) tuple_cleanup,
        nullptr,
        nullptr,
        (BoolBinOp) tuple_equal,
        (BoolUnaryOp) tuple_is_true,
        (SizeTUnaryOp) tuple_hash,
        (UnaryOp) tuple_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &tuple_sequence,
        nullptr
};

template<typename T>
Tuple *TupleClone(T *t) {
    Tuple *tuple;

    if ((tuple = TupleNew(t->len)) == nullptr)
        return nullptr;

    for (size_t i = 0; i < t->len; i++) {
        IncRef(t->objects[i]);
        tuple->objects[i] = t->objects[i];
    }

    tuple->len = t->len;
    return tuple;
}

Tuple *argon::object::TupleNew(size_t len) {
    auto tuple = ArObjectNew<Tuple>(RCType::INLINE, &type_tuple_);

    if (tuple != nullptr) {
        tuple->objects = nullptr;
        tuple->len = len;

        if (len > 0) {
            if ((tuple->objects = (ArObject **) argon::memory::Alloc(len * sizeof(void *))) == nullptr) {
                Release(tuple);
                return nullptr;
            }

            for (size_t i = 0; i < len; i++) {
                IncRef(NilVal);
                tuple->objects[i] = NilVal;
            }
        }
    }

    return tuple;
}

Tuple *argon::object::TupleNew(const ArObject *sequence) {
    if (IsSequence(sequence)) {
        if (AR_TYPEOF(sequence, type_list_))
            return TupleClone((List *) sequence);
        else if (AR_TYPEOF(sequence, type_tuple_))
            return TupleClone((Tuple *) sequence);
    }

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