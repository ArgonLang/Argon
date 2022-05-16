// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>
#include <cstring>

#include <object/datatype/support/bytesops.h>
#include <object/arobject.h>

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
        ArSize len;

        /* Number of graphemes in string */
        ArSize cp_len;

        /* String hash */
        ArSize hash;
    };

    extern const TypeInfo *type_string_;

    class StringBuilder {
        unsigned char *buffer_ = nullptr;

        ArSize cap_ = 0;
        ArSize len_ = 0;
        ArSize cp_len_ = 0;

        StringKind kind_ = StringKind::ASCII;
        bool error_ = false;

        static ArSize GetEscapedLength(const unsigned char *buffer, ArSize len, bool unicode);

    public:
        ~StringBuilder();

        bool BufferResize(ArSize sz);

        bool Write(const unsigned char *buffer, ArSize len, ArSize overalloc);

        bool Write(const String *string, ArSize overalloc) {
            return this->Write(string->buffer, string->len, overalloc);
        }

        bool WriteEscaped(const unsigned char *buffer, ArSize len, ArSize overalloc, bool unicode);

        bool WriteEscaped(const unsigned char *buffer, ArSize len, ArSize overalloc) {
            return this->WriteEscaped(buffer, len, overalloc, false);
        }

        bool WriteHex(const unsigned char *buffer, ArSize len);

        bool WriteRepeat(char ch, int times);

        String *BuildString();
    };

    // String

    String *StringNew(const char *string, ArSize len);

    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    inline String *StringNew(const std::string &string) { return StringNew(string.c_str(), string.length()); }

    String *StringNewBufferOwnership(unsigned char *buffer, ArSize len);

    String *StringNewFormat(const char *string, va_list vargs);

    String *StringNewFormat(const char *string, ...);

    String *StringIntern(const char *string, ArSize len);

    inline String *StringIntern(const char *str) { return StringIntern(str, strlen(str)); }

    inline String *StringIntern(const std::string &string) { return StringIntern(string.c_str(), string.length()); }

    // Common Operations

    ArObject *StringSplit(const String *string, const unsigned char *c_str, ArSize len, ArSSize maxsplit);

    inline ArObject *StringSplit(const String *string, const char *c_str, ArSSize maxsplit) {
        return StringSplit(string, (const unsigned char *) c_str, strlen(c_str), maxsplit);
    }

    inline ArObject *StringSplit(const String *string, const String *pattern, ArSSize maxsplit) {
        return StringSplit(string, pattern->buffer, pattern->len, maxsplit);
    }

    ArSize StringLen(const String *str);

    ArSize StringSubStrLen(const String *str, ArSize offset, ArSize graphemes);

    inline ArSSize StringFind(const String *string, const String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, false);
    }

    inline ArSSize StringFind(const String *string, const char *pattern) {
        return support::Find(string->buffer, string->len, (const unsigned char *) pattern, strlen(pattern), false);
    }

    inline ArSSize StringRFind(const String *string, const String *pattern) {
        return support::Find(string->buffer, string->len, pattern->buffer, pattern->len, true);
    }

    inline ArSSize StringRFind(const String *string, const char *pattern) {
        return support::Find(string->buffer, string->len, (const unsigned char *) pattern, strlen(pattern), true);
    }

    inline bool StringEmpty(const String *string) { return string->len == 0; }

    bool StringEndsWith(const String *string, const String *pattern);

    bool StringEq(const String *string, const unsigned char *c_str, ArSize len);

    int StringCompare(const String *left, const String *right);

    int StringIntToUTF8(unsigned int glyph, unsigned char *buf);

    int StringUTF8toInt(const unsigned char *buf);

    String *StringConcat(const String *left, const String *right);

    String *StringConcat(const String *left, const char *right, bool internal);

    String *StringFormat(const String *fmt, ArObject *args);

    String *StringCFormat(const char *fmt, ArObject *args);

    String *StringReplace(String *string, const String *old, const String *nval, ArSSize n);

    inline String *StringReplaceAll(String *string, const String *old, const String *nval) {
        return StringReplace(string, old, nval, -1);
    }

    String *StringSubs(const String *string, ArSize start, ArSize end);
}

#endif // !ARGON_OBJECT_STRING_H_
