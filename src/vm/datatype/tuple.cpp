// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "boolean.h"
#include "bounds.h"
#include "integer.h"
#include "list.h"
#include "nil.h"
#include "stringbuilder.h"

#include "tuple.h"

using namespace argon::vm::datatype;

ARGON_METHOD(tuple_find, find,
             "Find an item into the tuple and returns its position.\n"
             "\n"
             "- Parameter object: Object to search.\n"
             "- Returns: Index if the object was found into the tuple, -1 otherwise.\n",
             ": object", false, false) {
    const auto *tuple = (const Tuple *) _self;

    for (ArSize i = 0; i < tuple->length; i++) {
        if (Equal(tuple->objects[i], *args))
            return (ArObject *) IntNew((IntegerUnderlying) i);
    }

    return (ArObject *) IntNew(-1);
}

const FunctionDef tuple_methods[] = {
        tuple_find,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots tuple_objslot = {
        tuple_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *tuple_get_item(const Tuple *self, ArObject *index) {
    if (AR_TYPEOF(index, type_int_))
        return TupleGet(self, ((Integer *) index)->sint);
    else if (AR_TYPEOF(index, type_uint_))
        return TupleGet(self, (ArSSize) ((Integer *) index)->uint);

    ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                type_uint_->name, AR_TYPE_NAME(index));
    return nullptr;
}

ArObject *tuple_get_slice(const Tuple *self, ArObject *bounds) {
    auto *b = (Bounds *) bounds;
    Tuple *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(b, self->length, &start, &stop, &step);

    if ((ret = TupleNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step)
            ret->objects[i++] = IncRef(self->objects[start]);
    } else {
        for (ArSize i = 0; stop < start; start += step)
            ret->objects[i++] = IncRef(self->objects[start]);
    }

    return (ArObject *) ret;
}

ArObject *tuple_item_in(const Tuple *self, const ArObject *key) {
    for (ArSize i = 0; i < self->length; i++) {
        if (Equal(self->objects[i], key))
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

ArSize tuple_length(const Tuple *self) {
    return self->length;
}

const SubscriptSlots tuple_subscript = {
        (ArSize_UnaryOp) tuple_length,
        (BinaryOp) tuple_get_item,
        nullptr,
        (BinaryOp) tuple_get_slice,
        nullptr,
        (BinaryOp) tuple_item_in
};

ArObject *tuple_compare(const Tuple *self, const ArObject *other, CompareMode mode) {
    const auto *o = (const Tuple *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != o) {
        if (self->length != o->length)
            return BoolToArBool(false);

        for (ArSize i = 0; i < self->length; i++) {
            if (!Equal(self->objects[i], o->objects[i]))
                return BoolToArBool(false);
        }
    }

    return BoolToArBool(true);
}

ArObject *tuple_iter(Tuple *self, bool reverse) {
    auto *ti = MakeObject<TupleIterator>(type_tuple_iterator_);

    if (ti != nullptr) {
        new(&ti->lock)std::mutex();

        ti->iterable = IncRef(self);
        ti->index = 0;
        ti->reverse = reverse;
    }

    return (ArObject *) ti;
}

ArObject *tuple_repr(const Tuple *self) {
    StringBuilder builder;
    ArObject *ret;

    builder.Write((const unsigned char *) "(", 1, self->length == 0 ? 1 : 256);

    for (ArSize i = 0; i < self->length; i++) {
        auto *tmp = (String *) Repr(self->objects[i]);

        if (tmp == nullptr)
            return nullptr;

        if (!builder.Write(tmp, i + 1 < self->length ? (int) (self->length - i) + 2 : 1)) {
            Release(tmp);
            return nullptr;
        }

        if (i + 1 < self->length)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(tmp);
    }

    builder.Write((const unsigned char *) ")", 1, 0);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ArSize tuple_hash(Tuple *self) {
    unsigned long result = 1;
    ArSize hash;

    if (self->hash != 0)
        return self->hash;

    if (self->length == 0)
        return 0;

    for (ArSize i = 0; i < self->length; i++) {
        if (Hash(self->objects[i], &hash))
            return 0;

        result = 31 * result + hash;
    }

    self->hash = result;

    return self->hash;
}

bool tuple_dtor(Tuple *self) {
    for (ArSize i = 0; i < self->length; i++)
        Release(self->objects[i]);

    argon::vm::memory::Free(self->objects);

    return true;
}

bool tuple_is_true(const Tuple *self) {
    return self->length > 0;
}

TypeInfo TupleType = {
        AROBJ_HEAD_INIT_TYPE,
        "Tuple",
        nullptr,
        nullptr,
        sizeof(Tuple),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) tuple_dtor,
        nullptr,
        (ArSize_UnaryOp) tuple_hash,
        (Bool_UnaryOp) tuple_is_true,
        (CompareOp) tuple_compare,
        (UnaryConstOp) tuple_repr,
        nullptr,
        (UnaryBoolOp) tuple_iter,
        nullptr,
        nullptr,
        nullptr,
        &tuple_objslot,
        &tuple_subscript,
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

    ErrorFormat(kOverflowError[0], kOverflowError[1], type_tuple_->name, tuple->length, index);

    return nullptr;
}

bool argon::vm::datatype::TupleInsert(Tuple *tuple, ArObject *object, ArSize index) {
    if (index >= tuple->length)
        return false;

    Release(tuple->objects[index]);

    tuple->objects[index] = object == nullptr ? (ArObject *) Nil : IncRef(object);

    return true;
}

Tuple *argon::vm::datatype::TupleConvertList(argon::vm::datatype::List **list) {
    assert(AR_GET_RC(*list).GetStrongCount() == 1);

    auto *tuple = MakeObject<Tuple>(type_tuple_);

    if (tuple != nullptr) {
        tuple->objects = (*list)->objects;
        tuple->length = (*list)->length;

        (*list)->objects = nullptr;

        Release(list);
    }

    return tuple;
}

Tuple *TupleFromIterable(ArObject *iterable) {
    ArObject *iter;
    ArObject *tmp;
    List *ret;

    if ((iter = IteratorGet(iterable, false)) == nullptr)
        return nullptr;

    if ((ret = ListNew()) == nullptr) {
        Release(iter);
        return nullptr;
    }

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(iter);

            return nullptr;
        }

        Release(tmp);
    }

    Release(iter);

    auto *tuple = TupleConvertList(&ret);

    Release(ret);

    return tuple;
}

Tuple *argon::vm::datatype::TupleNew(ArObject *iterable) {
    Tuple *tuple;

    if (!AR_TYPEOF(iterable, type_list_) && !AR_TYPEOF(iterable, type_tuple_))
        return TupleFromIterable(iterable);

    if ((tuple = MakeObject<Tuple>(type_tuple_)) == nullptr)
        return nullptr;

    tuple->objects = nullptr;
    tuple->length = 0;
    tuple->hash = 0;

    if (AR_TYPEOF(iterable, type_list_)) {
        // List fast-path
        const auto *list = (List *) iterable;

        tuple->length = list->length;

        if (list->length > 0) {
            tuple->objects = (ArObject **) argon::vm::memory::Alloc(list->length * sizeof(void *));
            if (tuple->objects == nullptr) {
                Release(tuple);
                return nullptr;
            }

            for (ArSize i = 0; i < tuple->length; i++)
                tuple->objects[i] = IncRef(list->objects[i]);
        }

        return tuple;
    } else if (AR_TYPEOF(iterable, type_tuple_)) {
        // Tuple fast-path
        const auto *t1 = (Tuple *) iterable;

        tuple->length = t1->length;
        tuple->hash = t1->hash;

        if (t1->length > 0) {
            tuple->objects = (ArObject **) argon::vm::memory::Alloc(t1->length * sizeof(void *));
            if (tuple->objects == nullptr) {
                Release(tuple);
                return nullptr;
            }

            for (ArSize i = 0; i < tuple->length; i++)
                tuple->objects[i] = IncRef(t1->objects[i]);
        }

        return tuple;
    }

    ErrorFormat(kTypeError[0], kTypeError[8], AR_TYPE_NAME(iterable), type_tuple_->name);
    return nullptr;
}

Tuple *argon::vm::datatype::TupleNew(ArSize length) {
    auto *tuple = MakeObject<Tuple>(type_tuple_);

    if (tuple != nullptr) {
        tuple->objects = nullptr;
        tuple->length = length;
        tuple->hash = 0;

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

ArObject *tupleiterator_iter_next(TupleIterator *self) {
    std::unique_lock iter_lock(self->lock);

    if (!self->reverse) {
        if (self->index < self->iterable->length) {
            auto *tmp = IncRef(self->iterable->objects[self->index]);

            self->index++;

            return tmp;
        }
        return nullptr;
    }

    if (self->iterable->length - self->index == 0)
        return nullptr;

    self->index++;

    return IncRef(self->iterable->objects[self->iterable->length - self->index]);
}

bool tupleiterator_is_true(TupleIterator *self) {
    std::unique_lock iter_lock(self->lock);

    return (self->iterable->length - self->index) > 0;
}

TypeInfo TupleIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "TupleIterator",
        nullptr,
        nullptr,
        sizeof(List),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) IteratorDtor,
        nullptr,
        nullptr,
        (Bool_UnaryOp) tupleiterator_is_true,
        nullptr,
        nullptr,
        nullptr,
        (UnaryBoolOp) IteratorIter,
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
