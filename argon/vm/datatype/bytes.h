// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BYTES_H_
#define ARGON_VM_DATATYPE_BYTES_H_

#include <cstring>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/bufview.h>
#include <argon/vm/datatype/iterator.h>

namespace argon::vm::datatype {
    struct Bytes {
        AROBJ_HEAD;

        BufferView view;

        ArSize hash;

        void lock() const {
            this->view.shared->rwlock.lock();
        }

        void lock_shared() const {
            if (!this->view.shared->IsFrozen())
                this->view.shared->rwlock.lock_shared();
        }

        void unlock() const {
            this->view.shared->rwlock.unlock();
        }

        void unlock_shared() const {
            if (!this->view.shared->IsFrozen())
                this->view.shared->rwlock.unlock_shared();
        }
    };

    _ARGONAPI extern const TypeInfo *type_bytes_;

    using BytesIterator = Iterator<Bytes>;
    _ARGONAPI extern const TypeInfo *type_bytes_iterator_;

    /**
     * @brief Concatenate two bytes string.
     *
     * @param left Left bytes string.
     * @param right Right bytes string.
     * @return A pointer to an Argon bytes object, otherwise nullptr.
     */
    Bytes *BytesConcat(Bytes *left, Bytes *right);

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

    /**
     * @brief Create a new bytes object using the buffer parameter as an internal buffer.
     *
     * The new bytes object becomes the owner of the buffer passed as a parameter.
     *
     * @param buffer Buffer to use as an internal buffer.
     * @param cap Set buffer capacity.
     * @param len Dimension actually used by the data.
     * @param frozen Set if it is immutable.
     * @return A pointer to a bytes object, otherwise nullptr.
     */
    Bytes *BytesNewHoldBuffer(unsigned char *buffer, ArSize cap, ArSize len, bool frozen);

    /**
     * @brief Returns new bytes string where a specified value is replaced with a specified value.
     *
     * @param bytes Argon Bytes object.
     * @param old Bytes string to search for.
     * @param nval Bytes string to replace the old value with.
     * @param n Number specifying how many occurrences of the old value you want to replace.
     *          To replace all occurrence use -1.
     * @return Bytes string where a specified value is replaced.
     */
    Bytes *BytesReplace(Bytes *bytes, Bytes *old, Bytes *nval, ArSSize n);

    /**
     * @brief Removes any leading (spaces at the beginning) and trailing (spaces at the end) characters.
     *
     * @param bytes Argon bytes string.
     * @param buffer Optional. A set of characters to remove as leading/trailing characters.
     * @param length Buffer length, if the buffer is nullptr, the length must be zero.
     * @param left Removes any leading characters (default spaces).
     * @param right Removes any trailing characters (default spaces).
     * @return Returns a new bytes string stripped of characters in left/right or both ends.
     */
    Bytes *BytesTrim(Bytes *bytes, const unsigned char *buffer, ArSize length, bool left, bool right);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_BYTES_H_
