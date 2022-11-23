// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_TUPLE_H_
#define ARGON_VM_TUPLE_H_

#include "list.h"

#include "arobject.h"

namespace argon::vm::datatype {
    struct Tuple {
        AROBJ_HEAD;

        ArObject **objects;
        ArSize length;
        ArSize hash;
    };
    extern const TypeInfo *type_tuple_;

    struct TupleIterator {
        AROBJ_HEAD;

        Tuple *tuple;

        ArSize index;

        bool reverse;
    };
    extern const TypeInfo *type_tuple_iterator_;

    ArObject *TupleGet(const Tuple *tuple, ArSSize index);

    /**
     * @brief Inserts an element into the tuple at the indicated location.
     * @param tuple Tuple object.
     * @param object Object to insert.
     * @param index Location to insert the object.
     * @return True on success, false otherwise.
     */
    bool TupleInsert(Tuple * tuple, ArObject * object, ArSize index);

    /**
     * @brief Transform a list with only one reference into a tuple.
     *
     * This function allows you to use a List object to dynamically add elements and then return a Tuple
     * object by moving the internal data structure of the List inside the Tuple object.
     * To use this function, the List object must have only one active reference,
     * using it on a List with more than one active reference causes the application to crash.
     *
     * If the function is successful the List object is automatically released!
     *
     * @warning This is a convenience function, using this function incorrectly causes an immediate crash.
     *
     * @param list List to be converted.
     * @return A pointer to the newly created tuple object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Tuple *TupleConvertList(List **list);

    /**
     * @brief Create a new tuple from iterable.
     * @param iterable Pointer to an iterable object.
     * @return A pointer to the newly created tuple object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Tuple *TupleNew(ArObject *iterable);

    /**
     * @brief Create a new tuple.
     * @param length Length of the tuple.
     * @return A pointer to the newly created tuple object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Tuple *TupleNew(ArSize length);

    /**
     * @brief Create a new tuple.
     * @param objects C-Array to copy objects from.
     * @param count Length of the array.
     * @return A pointer to the newly created tuple object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Tuple *TupleNew(ArObject **objects, ArSize count);
}

#endif // !ARGON_VM_TUPLE_H_
