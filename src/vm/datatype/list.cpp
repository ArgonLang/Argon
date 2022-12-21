// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "boolean.h"
#include "bounds.h"
#include "stringbuilder.h"

#include "list.h"

using namespace argon::vm::datatype;

// Prototypes

List *ListFromIterable(List **dest, ArObject *iterable);

const FunctionDef list_methods[] = {
        ARGON_METHOD_SENTINEL
};

const ObjectSlots list_objslot = {
        list_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *list_get_item(List *self, ArObject *index) {
    if (AR_TYPEOF(index, type_int_))
        return ListGet(self, ((Integer *) index)->sint);
    else if (AR_TYPEOF(index, type_uint_))
        return ListGet(self, (ArSSize) ((Integer *) index)->uint);

    ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                type_uint_->name, AR_TYPE_NAME(index));
    return nullptr;
}

ArObject *list_get_slice(List *self, ArObject *bounds) {
    auto *b = (Bounds *) bounds;
    List *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    std::shared_lock _(self->rwlock);

    slice_len = BoundsIndex(b, self->length, &start, &stop, &step);

    if ((ret = ListNew(slice_len)) == nullptr)
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

ArObject *list_item_in(List *self, const ArObject *key) {
    std::shared_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++) {
        if (Equal(self->objects[i], key))
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

ArSize list_length(const Tuple *self) {
    return self->length;
}

bool list_set_item(List *self, ArObject *index, ArObject *value) {
    if (AR_TYPEOF(index, type_int_))
        return ListInsert(self, value, ((Integer *) index)->sint);
    else if (AR_TYPEOF(index, type_uint_))
        return ListInsert(self, value, (ArSSize) ((Integer *) index)->uint);

    ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                type_uint_->name, AR_TYPE_NAME(index));

    return false;
}

const SubscriptSlots list_subscript = {
        (ArSize_UnaryOp) list_length,
        (BinaryOp) list_get_item,
        (Bool_TernaryOp) list_set_item,
        (BinaryOp) list_get_slice,
        nullptr,
        (BinaryOp) list_item_in
};

bool CheckSize(List *list, ArSize count) {
    ArSize len = list->capacity + count;
    ArObject **tmp;

    if (count == 0)
        len = (list->capacity + 1) + ((list->capacity + 1) / 2);

    if (list->length + count > list->capacity) {
        if (list->objects == nullptr)
            len = kListInitialCapacity;

        if ((tmp = (ArObject **) argon::vm::memory::Realloc(list->objects, len * sizeof(void *))) == nullptr)
            return false;

        list->objects = tmp;
        list->capacity = len;
    }

    return true;
}

ArObject *list_compare(List *self, ArObject *other, CompareMode mode) {
    auto *o = (List *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock self_lock(self->rwlock);
    std::unique_lock other_lock(o->rwlock);

    if (self->length != o->length)
        return BoolToArBool(false);

    for (ArSize i = 0; i < self->length; i++) {
        if (!Equal(self->objects[i], o->objects[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *list_iter(List *self, bool reverse) {
    auto *li = MakeGCObject<ListIterator>(type_list_iterator_, true);

    if (li != nullptr) {
        new(&li->lock)std::mutex;

        li->iterable = IncRef(self);
        li->index = 0;
        li->reverse = reverse;
    }

    return (ArObject *) li;
}

ArObject *list_repr(List *self) {
    StringBuilder builder;
    ArObject *ret;

    int rec;
    if ((rec = RecursionTrack((ArObject *) self)) != 0)
        return rec > 0 ? (ArObject *) StringIntern("[...]") : nullptr;

    std::shared_lock _(self->rwlock);

    builder.Write((const unsigned char *) "[", 1, self->length == 0 ? 1 : 256);

    for (ArSize i = 0; i < self->length; i++) {
        auto *tmp = (String *) Repr(self->objects[i]);

        if (tmp == nullptr) {
            RecursionUntrack((ArObject *) self);
            return nullptr;
        }

        if (!builder.Write(tmp, i + 1 < self->length ? (int) (self->length - i) + 2 : 1)) {
            Release(tmp);

            RecursionUntrack((ArObject *) self);

            return nullptr;
        }

        if (i + 1 < self->length)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(tmp);
    }

    builder.Write((const unsigned char *) "]", 1, 0);

    RecursionUntrack((ArObject *) self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

bool list_dtor(List *self) {
    for (ArSize i = 0; i < self->length; i++)
        Release(self->objects[i]);

    argon::vm::memory::Free(self->objects);

    return true;
}

bool list_is_true(List *self) {
    std::shared_lock _(self->rwlock);

    return self->length > 0;
}

void list_trace(List *self, Void_UnaryOp trace) {
    std::shared_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++)
        trace(self->objects[i]);
}

TypeInfo ListType = {
        AROBJ_HEAD_INIT_TYPE,
        "List",
        nullptr,
        nullptr,
        sizeof(List),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) list_dtor,
        (TraceOp) list_trace,
        nullptr,
        (Bool_UnaryOp) list_is_true,
        (CompareOp) list_compare,
        (UnaryConstOp) list_repr,
        nullptr,
        (UnaryBoolOp) list_iter,
        nullptr,
        nullptr,
        nullptr,
        &list_objslot,
        &list_subscript,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_list_ = &ListType;

ArObject *argon::vm::datatype::ListGet(List *list, ArSSize index) {
    std::shared_lock _(list->rwlock);

    if (index < 0)
        index = (ArSSize) list->length + index;

    if (index >= 0 && index < list->length)
        return IncRef(list->objects[index]);

    ErrorFormat(kOverflowError[0], kOverflowError[1], type_tuple_->name, list->length, index);
    return nullptr;
}

bool argon::vm::datatype::ListAppend(List *list, ArObject *object) {
    std::unique_lock _(list->rwlock);

    if (!CheckSize(list, 1))
        return false;

    list->objects[list->length++] = IncRef(object);

    argon::vm::memory::TrackIf((ArObject *) list, object);

    return true;
}

bool argon::vm::datatype::ListExtend(List *list, ArObject *iterator) {
    if (iterator == nullptr)
        return true;

    if (!AR_TYPEOF(iterator, type_list_) && !AR_TYPEOF(iterator, type_tuple_))
        return ListFromIterable(&list, iterator) != nullptr;

    std::unique_lock _(list->rwlock);

    if (AR_TYPEOF(iterator, type_list_)) {
        // List fast-path
        auto *right = (List *) iterator;

        if (list != right)
            right->rwlock.lock_shared();

        if (!CheckSize(list, right->length)) {
            if (list != right)
                right->rwlock.unlock_shared();

            return false;
        }

        for (ArSize i = 0; i < right->length; i++) {
            auto *object = IncRef(right->objects[i]);

            list->objects[list->length + i] = object;

            argon::vm::memory::TrackIf((ArObject *) list, object);
        }

        list->length += right->length;

        if (list != right)
            right->rwlock.unlock_shared();

        return true;
    } else if (AR_TYPEOF(iterator, type_tuple_)) {
        // Tuple fast-path
        const auto *right = (const Tuple *) iterator;

        if (!CheckSize(list, right->length))
            return false;

        for (ArSize i = 0; i < right->length; i++) {
            auto *object = IncRef(right->objects[i]);

            list->objects[list->length + i] = object;

            argon::vm::memory::TrackIf((ArObject *) list, object);
        }

        list->length += right->length;

        return true;
    }

    assert(false);
}

bool argon::vm::datatype::ListExtend(List *list, ArObject **object, ArSize count) {
    std::unique_lock _(list->rwlock);

    if (!CheckSize(list, count))
        return false;

    for (ArSize i = 0; i < count; i++) {
        auto *obj = object[i];

        list->objects[list->length + i] = IncRef(obj);

        argon::vm::memory::TrackIf((ArObject *) list, obj);
    }

    list->length += count;

    return true;
}

bool argon::vm::datatype::ListInsert(List *list, ArObject *object, ArSSize index) {
    std::unique_lock _(list->rwlock);

    if (index < 0)
        index = ((ArSSize) list->length) + index;

    if (index > list->length) {
        if (!CheckSize(list, 1))
            return false;

        list->objects[list->length++] = IncRef(object);

        argon::vm::memory::TrackIf((ArObject *) list, object);

        return true;
    }

    Release(list->objects[index]);
    list->objects[index] = IncRef(object);

    argon::vm::memory::TrackIf((ArObject *) list, object);

    return true;
}

List *ListFromIterable(List **dest, ArObject *iterable) {
    ArObject *iter;
    ArObject *tmp;

    List *ret;

    if ((iter = IteratorGet(iterable, false)) == nullptr)
        return nullptr;

    if (dest == nullptr) {
        ret = ListNew();
        if (ret == nullptr) {
            Release(iter);
            return nullptr;
        }
    } else
        ret = *dest;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(iter);

            if (dest == nullptr)
                Release(ret);

            return nullptr;
        }

        Release(tmp);
    }

    Release(iter);

    return ret;
}

List *argon::vm::datatype::ListNew(ArSize capacity) {
    auto *list = MakeGCObject<List>(type_list_, false);

    if (list != nullptr) {
        list->objects = nullptr;

        if (capacity > 0) {
            list->objects = (ArObject **) argon::vm::memory::Alloc(capacity * sizeof(void *));
            if (list->objects == nullptr) {
                Release(list);
                return nullptr;
            }
        }

        list->capacity = capacity;
        list->length = 0;
    }

    return list;
}

List *argon::vm::datatype::ListNew(ArObject *iterable) {
    List *ret;

    if (!AR_TYPEOF(iterable, type_list_) && !AR_TYPEOF(iterable, type_tuple_))
        return ListFromIterable(nullptr, iterable);

    if (AR_TYPEOF(iterable, type_list_)) {
        // List fast-path
        auto *list = (List *) iterable;

        std::shared_lock _(list->rwlock);

        if ((ret = ListNew(list->length)) == nullptr)
            return nullptr;

        for (ArSize i = 0; i < list->length; i++)
            ret->objects[i] = IncRef(list->objects[i]);

        return ret;
    } else if (AR_TYPEOF(iterable, type_tuple_)) {
        // Tuple fast-path
        const auto *tuple = (Tuple *) iterable;

        if ((ret = ListNew(tuple->length)) == nullptr)
            return nullptr;

        for (ArSize i = 0; i < tuple->length; i++)
            ret->objects[i] = IncRef(tuple->objects[i]);

        return ret;
    }

    ErrorFormat(kTypeError[0], kTypeError[8], AR_TYPE_NAME(iterable), type_list_->name);
    return nullptr;
}

void argon::vm::datatype::ListRemove(List *list, ArSSize index) {
    std::unique_lock _(list->rwlock);

    if (index < 0)
        index = ((ArSSize) list->length) + index;

    if (index >= list->length)
        return;

    Release(list->objects[index]);

    // Move items back
    for (ArSize i = index + 1; i < list->length; i++)
        list->objects[i - 1] = list->objects[i];

    list->length--;
}

// LIST ITERATOR

ArObject *listiterator_iter_next(ListIterator *self) {
    if (self->iterable == nullptr)
        return nullptr;

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

bool listiterator_is_true(ListIterator *self) {
    std::unique_lock iter_lock(self->lock);
    std::shared_lock iterable_lock(self->iterable->rwlock);

    return (self->iterable->length - self->index) > 0;
}

TypeInfo ListIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "ListIterator",
        nullptr,
        nullptr,
        sizeof(ListIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) IteratorDtor,
        (TraceOp) IteratorTrace,
        nullptr,
        (Bool_UnaryOp) listiterator_is_true,
        nullptr,
        nullptr,
        nullptr,
        IteratorIter,
        (UnaryOp) listiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_list_iterator_ = &ListIteratorType;