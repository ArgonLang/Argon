// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "list.h"

using namespace argon::vm::datatype;

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

ArObject *list_iter(List *self, bool reverse) {
    auto *li = MakeGCObject<ListIterator>(type_list_iterator_);

    if (li != nullptr) {
        li->list = IncRef(self);
        li->index = 0;
        li->reverse = reverse;
    }

    return (ArObject *) li;
}

TypeInfo ListType = {
        AROBJ_HEAD_INIT_TYPE,
        "List",
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
        (UnaryBoolOp) list_iter,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_list_ = &ListType;

ArObject *argon::vm::datatype::ListGet(List *list, ArSSize index) {
    if (index < 0)
        index = (ArSSize) list->length + index;

    if (index >= 0 && index < list->length)
        return IncRef(list->objects[index]);

    // TODO: SetError?!

    return nullptr;
}

bool argon::vm::datatype::ListAppend(List *list, ArObject *object) {
    if (!CheckSize(list, 1))
        return false;

    list->objects[list->length++] = IncRef(object);
    // TODO: TrackIf
    return true;
}

bool argon::vm::datatype::ListExtend(List *list, ArObject *iterator) {
    if (AR_TYPEOF(iterator, type_list_)) {
        // Fast-path
        const auto *right = (const List *) iterator;

        if (!CheckSize(list, right->length))
            return false;

        for (ArSize i = 0; i < right->length; i++)
            list->objects[list->length + i] = IncRef(right->objects[i]);

        list->length += right->length;

        return true;
    }

    assert(false);
}

bool argon::vm::datatype::ListInsert(List *list, ArObject *object, ArSSize index) {
    if (index < 0)
        index = ((ArSSize) list->length) + index;

    if (index > list->length) {
        if (!CheckSize(list, 1))
            return false;

        list->objects[list->length++] = IncRef(object);
        // TODO: TrackIf
        return true;
    }

    Release(list->objects[index]);
    list->objects[index] = IncRef(object);
    // TODO: TrackIf
    return true;
}

List *argon::vm::datatype::ListNew(ArSize capacity) {
    auto *list = MakeGCObject<List>(type_list_);

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
    List *ret = nullptr;

    if (AR_TYPEOF(iterable, type_list_)) {
        // Fast-path
        const auto *list = (List *) iterable;

        if ((ret = ListNew(list->length)) == nullptr)
            return nullptr;

        for (ArSize i = 0; i < list->length; i++)
            ret->objects[i] = list->objects[i];

        return ret;
    }

    assert(false);

    return nullptr;
}

// LIST ITERATOR

ArObject *listiterator_iter(ListIterator *self, bool reversed) {
    if (self->reverse == reversed)
        return (ArObject *) IncRef(self);

    return list_iter(self->list, reversed);
}

ArObject *listiterator_iter_next(ListIterator *self) {
    if (!self->reverse) {
        if (self->index < self->list->length) {
            auto *tmp = IncRef(self->list->objects[self->index]);

            self->index++;

            return tmp;
        }
        return nullptr;
    }

    if (self->list->length - self->index == 0)
        return nullptr;

    self->index++;

    return IncRef(self->list->objects[self->list->length - self->index]);
}

TypeInfo ListIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "ListIterator",
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
        (UnaryBoolOp) listiterator_iter,
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