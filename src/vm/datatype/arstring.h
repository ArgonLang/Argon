// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ARSTRING_H_
#define ARGON_VM_DATATYPE_ARSTRING_H_

#include "arobject.h"

namespace argon::vm::datatype {
    enum class StringKind {
        ASCII,
        UTF8_2,
        UTF8_3,
        UTF8_4
    };

    struct String {
        AROBJ_HEAD;

        /* Raw buffer */
        unsigned char *buffer;

        /* String mode */
        StringKind kind;

        /* Interned string */
        bool intern;

        /* Length in bytes */
        ArSize length;

        /* Number of graphemes in string */
        ArSize cp_length;

        /* String hash */
        ArSize hash;
    };
    extern const TypeInfo *type_string_;

    /**
     * @brief Create new string.
     * @param string The C-string to convert to Argon string.
     * @param length The length of the C-string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringNew(const char *string, ArSize length);

    /**
     * @brief Create new string.
     * @param string The C-string to convert to Argon string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    /**
     * @brief Create a new string using a template.
     * @param format Printf style format string.
     * @param ... Arguments.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringFormat(const char *format, ...);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ARSTRING_H_
