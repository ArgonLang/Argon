// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LIST_H_
#define ARGON_VM_LIST_H_

#include "arobject.h"

namespace argon::vm::datatype {
    constexpr int kListInitialCapacity = 24;

    struct List {
        AROBJ_HEAD;

        ArObject **objects;
        ArSize capacity;
        ArSize length;
    };
    extern const TypeInfo *type_list_;

    struct ListIterator {
        AROBJ_HEAD;

        List *list;

        ArSize index;

        bool reverse;
    };
    extern const TypeInfo *type_list_iterator_;

    ArObject *ListGet(List *list, ArSSize index);

    /**
     * @brief Append object to the list.
     * @param list List object.
     * @param object Object to append.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListAppend(List *list, ArObject *object);

    /**
     * @brief Append the contents of the iterator to the list.
     * @param list List object.
     * @param iterator Iterator to append.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListExtend(List *list, ArObject *iterator);

    /**
     * @brief Insert element into the list.
     * @param list List object.
     * @param object Object to insert.
     * @param index Location to insert the object.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListInsert(List *list, ArObject *object, ArSSize index);

    /**
     * @brief Create a new list.
     * @param capacity Set initial capacity.
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    List *ListNew(ArSize capacity);

    /**
     * @brief Create a new list from iterable.
     * @param iterable Pointer to an iterable object.
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    List *ListNew(ArObject *iterable);

    /**
     * @brief Create a new list.
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    inline List *ListNew() { return ListNew(kListInitialCapacity); }
}

#endif // !ARGON_VM_LIST_H_
