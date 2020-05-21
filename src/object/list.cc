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

bool CheckSize(List *list) {
    ArObject **tmp = nullptr;

    if (list->len + 1 > list->cap) {
        tmp = (ArObject **) Realloc(list->objects, (list->cap + (list->cap / 2)) * sizeof(ArObject *));
        if (tmp == nullptr)
            return false;
        list->objects = tmp;
        list->cap += list->cap / 2;
    }

    return true;
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    if (!CheckSize(list)) {
        assert(false);
        return false;
    }
    IncRef(obj);
    list->objects[list->len] = obj;
    list->len++;
    return true;
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
    list->ref_count =  ARGON_OBJECT_REFCOUNT_INLINE;
    list->type = &type_list_;

    list->objects = (ArObject **) Alloc(cap * sizeof(ArObject *));
    assert(list->objects != nullptr);
    list->len = 0;
    list->cap = cap;
    return list;
}

List *argon::object::ListNew(const ArObject *sequence) {
    assert(false); // TODO: impl
}