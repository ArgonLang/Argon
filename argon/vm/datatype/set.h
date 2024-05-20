// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_SET_H_
#define ARGON_VM_DATATYPE_SET_H_

#include <argon/vm/sync/rsm.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/hashmap.h>

namespace argon::vm::datatype {

    using SetEntry = HEntry<ArObject, bool>;

    struct Set {
        AROBJ_HEAD;

        sync::RecursiveSharedMutex rwlock;
        
        HashMap<ArObject, bool> set;
    };
    _ARGONAPI extern const TypeInfo *type_set_;

    using SetIterator = CursorIterator<Set, SetEntry>;
    _ARGONAPI extern const TypeInfo *type_set_iterator_;

    /**
     * @brief Add element to set.
     *
     * @param set Set object.
     * @param object Object to insert.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool SetAdd(Set *set, ArObject *object);

    /**
     * @brief Check if an element is inside the set.
     *
     * @param set Set object.
     * @param object Object to insert.
     * @return True if the object is in the set, false otherwise
     */
    bool SetContains(Set *set, ArObject *object);

    bool SetMerge(Set *set, Set *other);

    /**
     * @brief Calculate the difference between two sets.
     *
     * @param left Left object.
     * @param right Right object.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetDifference(Set *left, Set *right);

    /**
     * @brief Calculate the intersection between two sets.
     *
     * @param left Left object.
     * @param right Right object.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetIntersection(Set *left, Set *right);

    /**
     * @brief Create a new set from an iterable object.
     *
     * @param left Left object.
     * @param right Right object.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetNew(ArObject *iterable);

    /**
     * @brief Create a new empty set.
     *
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetNew();

    /**
     * @brief Calculate the symmetric difference between two sets.
     *
     * @param left Left object.
     * @param right Right object.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetSymmetricDifference(Set *left, Set *right);

    /**
     * @brief Calculate union between two sets.
     *
     * @param left Left object.
     * @param right Right object.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Set *SetUnion(Set *left, Set *right);

    /**
     * @brief Removes all elements within the set.
     *
     * @param set Set object.
     */
    void SetClear(Set *set);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_SET_H_
