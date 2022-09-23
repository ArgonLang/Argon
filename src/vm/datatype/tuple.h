// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_TUPLE_H_
#define ARGON_VM_TUPLE_H_

#include "arobject.h"

namespace argon::vm::datatype {
    struct Tuple {
        AROBJ_HEAD;

        ArObject **objects;
        ArSize length;
        ArSize hash;
    };
    extern const TypeInfo *type_tuple_;

    /**
     * @brief Inserts an element into the tuple at the indicated location.
     * @param tuple Tuple object.
     * @param object Object to insert.
     * @param index Location to insert the object.
     * @return True on success, false otherwise.
     */
    bool TupleInsert(Tuple * tuple, ArObject * object, ArSize index);

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
