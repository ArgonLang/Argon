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

const TypeInfo ListType = {
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
const TypeInfo *argon::vm::datatype::type_list_ = &ListType;

bool argon::vm::datatype::ListAppend(List *list, ArObject *object) {
    if (!CheckSize(list, 1))
        return false;

    list->objects[list->length++] = IncRef(object);
    // TODO: TrackIf
    return true;
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