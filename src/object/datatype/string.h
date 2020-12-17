// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>
#include <cstring>

#include <object/arobject.h>
#include <object/datatype/support/bytesops.h>

#define AROBJECT_STR(obj)   ((argon::object::String *)obj->type->str(obj))

namespace argon::object {
    enum class StringKind {
        ASCII,
        UTF8_2,
        UTF8_3,
        UTF8_4
    };

    struct String : public ArObject {
        /* Raw buffer */
        unsigned char *buffer;

        /* String mode */
        StringKind kind;

        /* Interned string */
        bool intern;

        /* Length in bytes */
        size_t len;

        /* Number of graphemes in string */
        size_t cp_len;

        /* String hash */
        size_t hash;
    };

    extern const TypeInfo type_string_;

    String *StringNew(const char *string, size_t len);

    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    inline String *StringNew(const std::string &string) { return StringNew(string.c_str(), string.length()); }

    String *StringIntern(const char *string, size_t len);

    inline String *StringIntern(const char *str) { return StringIntern(str, strlen(str)); }

    inline String *StringIntern(const std::string &string) { return StringIntern(string.c_str(), string.length()); }

    // Common Operations

    inline bool StringEmpty(String *string) { return string->len == 0; }

    bool StringEq(String *string, const unsigned char *c_str, size_t len);

    size_t StringLen(const String *str);

    String *StringConcat(String *left, String *right);

    inline ArSSize StringFind(String *string, String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, false);
    }

    inline ArSSize StringRFind(String *string, String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, true);
    }

    String *StringReplace(String *string, String *old, String *newval, ArSSize n);

    inline String *StringReplaceAll(String *string, String *old, String *newval) {
        return StringReplace(string, old, newval, -1);
    }

    String *StringSubs(String *string, size_t start, size_t end);
}

#endif // !ARGON_OBJECT_STRING_H_
