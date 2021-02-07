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
        size_t cap;
        size_t len;
    };

    extern const TypeInfo type_list_;

    List *ListNew(size_t cap);

    List *ListNew(const ArObject *sequence);

    inline List *ListNew() { return ListNew(ARGON_OBJECT_LIST_INITIAL_CAP); }

    bool ListAppend(List *list, ArObject *obj);

    bool ListConcat(List *list, ArObject *sequence);

    void ListClear(List *list);

    void ListRemove(List *list, ArSSize i);

    ArObject *ListGetItem(List *list, ArSSize i);

} // namespace argon::object

#endif // !ARGON_OBJECT_LIST_H_
