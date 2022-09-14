// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ARSTRING_H_
#define ARGON_VM_DATATYPE_ARSTRING_H_

#include <cstdarg>
#include <cstring>

#include "arobject.h"

#define ARGON_RAW_STRING(string)        ((string)->buffer)
#define ARGON_RAW_STRING_LENGTH(string) ((string)->length)

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

    ArSize StringSubstrLen(const String *string, ArSize offset, ArSize graphemes);

    inline bool StringIsEmpty(const String *string) {
        return string->length == 0;
    }

    /**
     * @brief Concatenate two string.
     *
     * @param left Left string.
     * @param right Right string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringConcat(const String *left, const String *right);

    /**
     * @brief Concatenate Argon string to a C-string.
     *
     * @param left Argon string.
     * @param right C-string.
     * @param length Length of C-string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringConcat(const String *left, const char *string, ArSize length);

    /**
     * @brief Create a new string using a template.
     *
     * @param format Printf style format string.
     * @param ... Arguments.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringFormat(const char *format, ...);

    /**
     * @brief Create a new string using a template.
     *
     * @param format Printf style format string.
     * @param args Arguments.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringFormat(const char *format, va_list args);

    /**
     * @brief Creates an exact copy of a String object in the String pool and return it.
     *
     * @param string The C-string to convert to Argon string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringIntern(const char *string);

    /**
     * @brief Create new string.
     *
     * @param string The C-string to convert to Argon string.
     * @param length The length of the C-string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringNew(const char *string, ArSize length);

    /**
     * @brief Create new string.
     *
     * @param string The C-string to convert to Argon string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    /**
     * @brief Create new string.
     *
     * It allows you to build an empty String object (container only)
     * which must subsequently be filled by the applicant.
     *
     * @param buffer Raw buffer containing the string
     * (ownership of the buffer will be transferred to the created object).
     * @param length Length of the buffer.
     * @param cp_length Number of unicode code point in the buffer.
     * @param kind StringKind.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringNew(unsigned char *buffer, ArSize length, ArSize cp_length, StringKind kind);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ARSTRING_H_
