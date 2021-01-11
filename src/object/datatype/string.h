// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>
#include <cstring>

#include <utils/enum_bitmask.h>

#include <object/arobject.h>
#include <object/datatype/support/bytesops.h>

namespace argon::object {
    enum class StringFormatFlags {
        LJUST = 0x01,
        SIGN = 0x02,
        BLANK = 0x04,
        ALT = 0x08,
        ZERO = 0x10
    };

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

    struct StringBuilder {
        String str;
        size_t w_idx;
    };

    struct StringArg {
        StringFormatFlags flags;
        int prec;
        int width;
    };

    struct StringFormatter {
        struct {
            unsigned char *buf;
            size_t len;
            size_t idx;
        } fmt;

        StringBuilder builder;

        ArObject *args;
        size_t args_idx;
        size_t args_len;
        int nspec;
    };

    extern const TypeInfo type_string_;

    bool StringBuilderResize(StringBuilder *sb, size_t len);

    bool StringBuilderResizeAscii(StringBuilder *sb, const unsigned char *buffer, size_t len, int overalloc);

    int StringBuilderRepeat(StringBuilder *sb, char chr, int times);

    int StringBuilderWrite(StringBuilder *sb, const unsigned char *buffer, size_t len, int overalloc);

    inline int StringBuilderWrite(StringBuilder *sb, const unsigned char *buffer, size_t len) {
        return StringBuilderWrite(sb, buffer, len, 0);
    }

    inline int StringBuilderWrite(StringBuilder *sb, String *str, int overalloc) {
        return StringBuilderWrite(sb, str->buffer, str->len, overalloc);
    }

    int StringBuilderWriteAscii(StringBuilder *sb, const unsigned char *buffer, size_t len);

    String *StringBuilderFinish(StringBuilder *sb);

    void StringBuilderClean(StringBuilder *sb);

    // String

    String *StringNew(const char *string, size_t len);

    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    inline String *StringNew(const std::string &string) { return StringNew(string.c_str(), string.length()); }

    String *StringNewFormat(const char *string, ...);

    String *StringIntern(const char *string, size_t len);

    inline String *StringIntern(const char *str) { return StringIntern(str, strlen(str)); }

    inline String *StringIntern(const std::string &string) { return StringIntern(string.c_str(), string.length()); }

    // Common Operations

    inline bool StringEmpty(String *string) { return string->len == 0; }

    bool StringEq(String *string, const unsigned char *c_str, size_t len);

    int StringIntToUTF8(unsigned int glyph, unsigned char *buf);

    size_t StringLen(const String *str);

    String *StringConcat(String *left, String *right);

    String *StringFormat(String *fmt, ArObject *args);

    String *StringCFormat(const char *fmt, ArObject *args);

    inline ArSSize StringFind(String *string, String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, false);
    }

    inline ArSSize StringRFind(String *string, String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, true);
    }

    String *StringReplace(String *string, String *old, String *nval, ArSSize n);

    inline String *StringReplaceAll(String *string, String *old, String *nval) {
        return StringReplace(string, old, nval, -1);
    }

    String *StringSubs(String *string, size_t start, size_t end);
}

ENUMBITMASK_ENABLE(argon::object::StringFormatFlags);

#endif // !ARGON_OBJECT_STRING_H_
