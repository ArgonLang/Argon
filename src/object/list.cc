// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "list.h"

#include <memory/memory.h>

using namespace argon::object;
using namespace argon::memory;

bool list_equal(ArObject *self, ArObject *other) {
    return false;
}

size_t list_hash(ArObject *obj) {
    return 0;
}

size_t list_len(ArObject *obj) {
    return ((List *) obj)->len;
}

ArObject *argon::object::ListGetItem(List *list, arsize i) {
    ArObject *obj;

    if (i >= list->len)
        return nullptr;

    obj = list->objects[i];
    IncRef(obj);
    return obj;
}

void list_cleanup(ArObject *obj) {
    auto list = (List *) obj;
    for (size_t i = 0; i < list->len; i++)
        Release(list->objects[i]);
}

bool CheckSize(List *list, size_t count) {
    ArObject **tmp;

    if (list->len + count > list->cap) {

        if (list->objects != nullptr) {
            if (count > 1)
                tmp = (ArObject **) Realloc(list->objects, (list->cap + count) * sizeof(ArObject *));
            else
                tmp = (ArObject **) Realloc(list->objects, (list->cap + (list->cap / 2)) * sizeof(ArObject *));
        } else {
            if (count > 1)
                tmp = (ArObject **) Alloc(count * sizeof(ArObject *));
            else
                tmp = (ArObject **) Alloc(ARGON_OBJECT_LIST_INITIAL_CAP * sizeof(ArObject *));
        }


        if (tmp == nullptr)
            return false;

        list->objects = tmp;
        list->cap += list->cap / 2;
    }

    return true;
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    if (!CheckSize(list, 1)) {
        assert(false);
        return false;
    }
    IncRef(obj);
    list->objects[list->len] = obj;
    list->len++;
    return true;
}

bool argon::object::ListConcat(List *list, ArObject *sequence) {
    if (IsSequence(sequence)) {
        if (sequence->type == &type_list_) {
            auto other = (List *) sequence;

            if (!CheckSize(list, other->len))
                return false;

            for (size_t i = 0; i < other->len; i++) {
                IncRef(other->objects[i]);
                list->objects[list->len + i] = other->objects[i];
            }

            list->len+=other->len;
            return true;
        }
    }
    assert(false); // TODO: impl
}

void argon::object::ListRemove(List *list, arsize i) {
    if (i >= list->len)
        return;

    Release(list->objects[i]);
    for (size_t idx = i + 1; idx < list->len; idx++)
        list->objects[idx - 1] = list->objects[idx];

    list->len--;
}

const SequenceActions list_actions{
        list_len,
        (BinaryOpArSize) argon::object::ListGetItem,
};

const TypeInfo argon::object::type_list_ = {
        (const unsigned char *) "list",
        sizeof(List),
        nullptr,
        &list_actions,
        nullptr,
        nullptr,
        list_equal,
        nullptr,
        list_hash,
        nullptr,
        list_cleanup
};

List *argon::object::ListNew() {
    return ListNew(ARGON_OBJECT_LIST_INITIAL_CAP);
}

List *argon::object::ListNew(size_t cap) {
    assert(cap > 0);
    auto list = (List *) Alloc(sizeof(List));
    list->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
    list->type = &type_list_;
    list->objects = nullptr;

    if (cap > 0)
        list->objects = (ArObject **) Alloc(cap * sizeof(ArObject *));

    assert(list->objects != nullptr);
    list->len = 0;
    list->cap = cap;
    return list;
}

List *argon::object::ListNew(const ArObject *sequence) {
    List *list;
    ArObject *tmp;

    if (IsSequence(sequence)) {
        if (sequence->type == &type_list_) {
            // List clone
            auto other = (List *) sequence;

            if ((list = ListNew(other->len)) == nullptr)
                return nullptr;

            for (size_t i = 0; i < other->len; i++) {
                tmp = (ArObject *) other->objects[i];
                IncRef(tmp);
                list->objects[i] = tmp;
            }

            list->len = other->len;

            return list;
        }
    }
    assert(false); // TODO: impl
}