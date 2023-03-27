// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_LIST_H_
#define ARGON_VM_DATATYPE_LIST_H_

#include <argon/vm/sync/rsm.h>

#include <argon/vm/datatype/iterator.h>
#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    constexpr int kListInitialCapacity = 24;

    struct List {
        AROBJ_HEAD;

        sync::RecursiveSharedMutex rwlock;

        ArObject **objects;

        ArSize capacity;

        ArSize length;
    };
    extern const TypeInfo *type_list_;

    using ListIterator = Iterator<List>;
    extern const TypeInfo *type_list_iterator_;

    /**
     * @brief Retrieves the item from the list at a given index.
     *
     * @param list Pointer to an instance of list.
     * @param index Index of the element to delete.
     * @return Pointer to element if successful, otherwise return nullptr and panic state will be set.
     */
    ArObject *ListGet(List *list, ArSSize index);

    /**
     * @brief Append object to the list.
     *
     * @param list List object.
     * @param object Object to append.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListAppend(List *list, ArObject *object);

    /**
     * @brief Append the contents of the iterator to the list.
     *
     * @param list List object.
     * @param iterator Iterator to append.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListExtend(List *list, ArObject *iterator);

    /**
    * @brief Append the contents of the ArObject** to the list.
    *
    * @param list List object.
    * @param object Array of objects to be append.
    * @param count Number of elements in the object array.
    * @return True on success, in case of error false will be returned and the panic state will be set.
    */
    bool ListExtend(List *list, ArObject **object, ArSize count);

    /**
     * @brief Insert element into the list.
     *
     * @param list List object.
     * @param object Object to insert.
     * @param index Location to insert the object.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListInsert(List *list, ArObject *object, ArSSize index);

    /**
     * @brief Prepend object to the list.
     *
     * @param list List object.
     * @param object Object to prepend list.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool ListPrepend(List *list, ArObject *object);

    /**
     * @brief Create a new list.
     *
     * @param capacity Set initial capacity.
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    List *ListNew(ArSize capacity);

    /**
     * @brief Create a new list from iterable.
     *
     * @param iterable Pointer to an iterable object.
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    List *ListNew(ArObject *iterable);

    /**
     * @brief Create a new list.
     *
     * @return A pointer to the newly created list object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    inline List *ListNew() { return ListNew(kListInitialCapacity); }

    /**
     * @brief Delete the contents of the entire list.
     *
     * @param list Pointer to an instance of list.
     */
    void ListClear(List *list);

    /**
     * @brief Remove an element from the list.
     *
     * @param list Pointer to an instance of list.
     * @param index Index of the element to delete.
     */
    void ListRemove(List *list, ArSSize index);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_LIST_H_
