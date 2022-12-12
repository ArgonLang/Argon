// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BYTES_H_
#define ARGON_VM_DATATYPE_BYTES_H_

#include <cstring>

#include "arobject.h"
#include "bufview.h"
#include "iterator.h"

namespace argon::vm::datatype {
    struct Bytes {
        AROBJ_HEAD;

        BufferView view;

        ArSize hash;

        bool frozen;
    };
    extern const TypeInfo *type_bytes_;

    using BytesIterator = Iterator<Bytes>;
    extern const TypeInfo *type_bytes_iterator_;

    /**
     * @brief Return a frozen bytes object.
     *
     * @param bytes Bytes object to freeze.
     * @return A pointer to a frozen bytes object, otherwise nullptr.
     */
    Bytes *BytesFreeze(Bytes *bytes);

    /**
     * @brief Create a new bytes object from a bufferable object.
     *
     * @param object Bufferable object.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    Bytes *BytesNew(ArObject *object);

    /**
     * @brief Create a new bytes object.
     *
     * @param cap Internal buffer capacity.
     * @param same_len Sets whether the length of the byte string equals its capacity.
     * @param fill_zero Should be filled with zeros.
     * @param frozen Set if it is immutable.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    Bytes *BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen);

    /**
     * @brief Create a new bytes object.
     *
     * @param buffer Input buffer to copy into byte string.
     * @param len Length of input buffer.
     * @param frozen Set if it is immutable.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    Bytes *BytesNew(const unsigned char *buffer, ArSize len, bool frozen);

    /**
     * @brief Create a new bytes object from an existing one.
     *
     * Internally a new view is created on the buffer of the bytes object passed as input.
     * No memory allocation is done.
     *
     * @param bytes Bytes string already exists.
     * @param start Bytes string start offset.
     * @param length Length of the view.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    Bytes *BytesNew(Bytes *bytes, ArSize start, ArSize length);

    /**
     * @brief Create a new bytes object.
     *
     * @param buffer Input buffer to copy into byte string.
     * @param length Length of input buffer.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    inline Bytes *BytesNew(const unsigned char *buffer, ArSize length) {
        return BytesNew(buffer, length, true);
    }

    /**
     * @brief Create a new bytes object.
     *
     * @param string C-string to copy into byte string.
     * @param frozen Set if it is immutable.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    inline Bytes *BytesNew(const char *string, bool frozen) {
        return BytesNew((const unsigned char *) string, strlen(string), frozen);
    }
}

#endif // !ARGON_VM_DATATYPE_BYTES_H_
