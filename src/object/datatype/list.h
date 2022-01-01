// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_LIST_H_
#define ARGON_OBJECT_LIST_H_

#include <object/arobject.h>

#define ARGON_OBJECT_LIST_INITIAL_CAP   4

namespace argon::object {
    struct List : ArObject {
        ArObject **objects;
        ArSize cap;
        ArSize len;
    };

    extern const TypeInfo *type_list_;

    List *ListNew(ArSize cap);

    List *ListNew(const ArObject *object);

    inline List *ListNew() { return ListNew(ARGON_OBJECT_LIST_INITIAL_CAP); }

    bool ListAppend(List *list, ArObject *obj);

    bool ListConcat(List *list, ArObject *sequence);

    bool ListInsertAt(List *list, ArObject *obj, ArSSize index);

    bool ListSetItem(List *list, ArObject *obj, ArSSize index);

    void ListClear(List *list);

    void ListRemove(List *list, ArSSize i);

    ArObject *ListGetItem(List *list, ArSSize i);

} // namespace argon::object

#endif // !ARGON_OBJECT_LIST_H_
