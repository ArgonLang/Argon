// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ARSTRING_H_
#define ARGON_VM_DATATYPE_ARSTRING_H_

#include <cstdarg>
#include <cstring>

#include <vm/datatype/support/byteops.h>

#include "iterator.h"
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

    using StringIterator = Iterator<String>;
    extern const TypeInfo *type_string_iterator_;

    /**
     * @brief Splits the string at the specified separator, and returns a list.
     *
     * @param string String to split.
     * @param pattern Specifies the separator to use when splitting the string.
     * @param plen Pattern length.
     * @param maxsplit Specifies how many splits to do.
     * @return New list of string.
     */
    ArObject *StringSplit(const String *string, const unsigned char *pattern, ArSize plen, ArSSize maxsplit);

    /**
     * @bref Returns the length of a unicode substring.
     *
     * This function is useful for taking substrings of length n from a unicode string,
     * indicating the number of graphemes instead of the length in bytes,
     * the function takes care of returning the correct length in bytes.
     *
     * @param string Argon string.
     * @param offset String offset.
     * @param graphemes Number of graphemes to include in the substring.
     * @return Returns the length in bytes that is needed to correctly allocate the content of the substring.
     */
    ArSize StringSubstrLen(const String *string, ArSize offset, ArSize graphemes);

    /**
     * @brief Search for a string within a string.
     *
     * @param string Argon string.
     * @param pattern Argon string containing the pattern to search for.
     * @return Returns the index at which the pattern value was found, otherwise -1.
     */
    inline ArSSize StringFind(const String *string, const String *pattern) {
        return support::Find(string->buffer, string->length, pattern->buffer, pattern->length, false);
    }

    /**
     * @brief Searches the string for a specified value and returns the last position of where it was found.
     *
     * @param string Argon string.
     * @param pattern Argon string containing the pattern to search for.
     * @return Returns the index at which the pattern value was found, otherwise -1.
     */
    inline ArSSize StringRFind(const String *string, const String *pattern) {
        return support::Find(string->buffer, string->length, pattern->buffer, pattern->length, true);
    }

    /**
     * @brief Returns true if the string ends with the specified value.
     *
     * @param string Argon string.
     * @param pattern The value to check if the string ends with.
     * @return True if the string ends with the specified value, false otherwise.
     */
    bool StringEndswith(const String *string, const String *pattern);

    /**
     * @brief Check if two strings are equal
     *
     * @param string Argon string.
     * @param c_str C-string.
     * @return Returns true if the strings are equal, false otherwise.
     */
    inline bool StringEqual(const String *string, const char *c_str) {
        return strcmp((const char *) ARGON_RAW_STRING(string), c_str) == 0;
    }

    /**
     * @brief Check if string is empty.
     *
     * @param string Argon string.
     * @return Returns true if the string is empty, false otherwise.
     */
    inline bool StringIsEmpty(const String *string) {
        return string->length == 0;
    }

    /**
     * @brief Compares two strings lexicographically.
     *
     * @param left Left Argon string.
     * @param right Right Argon string.
     * @return An int value:
     * 0 if the string is equal to the other string.
     * < 0 if the string is lexicographically less than the other string.
     * > 0 if the string is lexicographically greater than the other string (more characters).
     */
    int StringCompare(const String *left, const String *right);

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
     * @param length The length of the C-string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    String *StringIntern(const char *string, ArSize length);

    /**
     * @brief Creates an exact copy of a String object in the String pool and return it.
     *
     * @param string The C-string to convert to Argon string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    inline String *StringIntern(const char *string) {
        return StringIntern(string, strlen(string));
    }

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
     * @param string The unsigned C-string to convert to Argon string.
     * @param length The length of the unsigned C-string.
     * @return A pointer to an Argon string object, otherwise nullptr.
     */
    inline String *StringNew(const unsigned char *string, ArSize length) {
        return StringNew((const char *) string, length);
    }

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

    /**
     * @brief Returns a string where a specified value is replaced with a specified value.
     *
     * @param string Argon string.
     * @param old String to search for.
     * @param nval String to replace the old value with.
     * @param n Number specifying how many occurrences of the old value you want to replace.
     *          To replace all occurrence use -1.
     * @return String where a specified value is replaced.
     */
    String *StringReplace(String *string, const String *old, const String *nval, ArSSize n);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ARSTRING_H_
