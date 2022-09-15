// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>

#include "boolean.h"
#include "dict.h"
#include "hash_magic.h"
#include "stringbuilder.h"
#include "arstring.h"

using namespace argon::vm::datatype;

#define STR_BUF(str) ((str)->buffer)
#define STR_LEN(str) ((str)->length)

static Dict *intern = nullptr;

String *StringInit(ArSize len, bool mkbuf) {
    auto str = MakeObject<String>(type_string_);

    if (str != nullptr) {
        str->buffer = nullptr;

        if (mkbuf) {
            // +1 is '\0'
            str->buffer = (unsigned char *) argon::vm::memory::Alloc(len + 1);
            if (str->buffer == nullptr) {
                Release(str);
                return nullptr;
            }

            // Set terminator
            STR_BUF(str)[(len + 1) - 1] = 0x00;
        }

        str->kind = StringKind::ASCII;
        str->intern = false;
        STR_LEN(str) = len;
        str->cp_length = 0;
        str->hash = 0;
    }

    return str;
}

ArObject *string_compare(const String *self, const ArObject *other, CompareMode mode) {
    const auto *o = (const String *) other;
    int left = 0;
    int right = 0;
    int res;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    if (mode == CompareMode::EQ && self->kind != o->kind)
        return BoolToArBool(false);

    res = StringCompare(self, o);
    if (res < 0)
        left = -1;
    else if (res > 0)
        right = -1;

    ARGON_RICH_COMPARE_CASES(left, right, mode)
}


ArSize string_hash(String *self){
    if (self->hash == 0)
        self->hash = HashBytes(self->buffer, self->length);

    return self->hash;
}

const TypeInfo StringType = {
        AROBJ_HEAD_INIT_TYPE,
        "String",
        nullptr,
        nullptr,
        sizeof(String),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) string_hash,
        nullptr,
        (CompareOp) string_compare,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_string_ = &StringType;

ArSize argon::vm::datatype::StringSubstrLen(const String *string, ArSize offset, ArSize graphemes) {
    const unsigned char *buf = STR_BUF(string) + offset;
    const unsigned char *end = STR_BUF(string) + string->length;

    if (graphemes == 0)
        return 0;

    while (graphemes-- && buf < end) {
        if (*buf >> 7u == 0x0)
            buf += 1;
        else if (*buf >> 5u == 0x6)
            buf += 2;
        else if (*buf >> 4u == 0xE)
            buf += 3;
        else if (*buf >> 3u == 0x1E)
            buf += 4;
    }

    return buf - (STR_BUF(string) + offset);
}

int argon::vm::datatype::StringCompare(const String *left, const String *right) {
    ArSize idx1 = 0;
    ArSize idx2 = 0;

    unsigned char c1;
    unsigned char c2;

    do {
        c1 = ARGON_RAW_STRING(left)[idx1++];
        c2 = ARGON_RAW_STRING(right)[idx2++];

        // Take care of '\0', String->len not include '\0'
    } while (c1 == c2 && idx1 < ARGON_RAW_STRING_LENGTH(left) + 1);

    return c1 - c2;
}

String *argon::vm::datatype::StringConcat(const String *left, const String *right) {
    String *ret = StringInit(STR_LEN(left) + STR_LEN(right), true);

    if (ret != nullptr) {
        memory::MemoryCopy(ret->buffer, STR_BUF(left), STR_LEN(left));
        memory::MemoryCopy(ret->buffer + STR_LEN(left), STR_BUF(right), STR_LEN(right));

        ret->kind = left->kind;

        if (right->kind > left->kind)
            ret->kind = right->kind;

        ret->cp_length = left->cp_length + right->cp_length;
    }

    return ret;
}

String *argon::vm::datatype::StringConcat(const String *left, const char *string, ArSize length) {
    assert(false);
}

String *argon::vm::datatype::StringFormat(const char *format, ...) {
    va_list args;
    String *str;

    va_start(args, format);
    str = StringFormat(format, args);
    va_end(args);

    return str;
}

String *argon::vm::datatype::StringFormat(const char *format, va_list args) {
    va_list vargs2;
    String *str;
    int sz;

    va_copy(vargs2, args);
    sz = vsnprintf(nullptr, 0, format, vargs2) + 1; // +1 is for '\0'
    va_end(vargs2);

    if ((str = StringInit(sz - 1, true)) == nullptr)
        return nullptr;

    str->cp_length = sz - 1;

    va_copy(vargs2, args);
    vsnprintf((char *) STR_BUF(str), sz, format, vargs2);
    va_end(vargs2);

    return str;
}

String *argon::vm::datatype::StringIntern(const char *string) {
    String *ret;

    // Initialize intern
    if (intern == nullptr) {
        intern = DictNew();
        if (intern == nullptr)
            return nullptr;

        // Insert empty string
        if ((ret = StringNew("", 0)) == nullptr)
            return nullptr;

        if (!DictInsert(intern, (ArObject *) ret, (ArObject *) ret)) {
            Release(ret);
            return nullptr;
        }

        ret->intern = true;

        if (string == nullptr || strlen(string) == 0)
            return ret;

        Release(ret);
    }

    if ((ret = (String *) DictLookup(intern, string)) == nullptr) {
        if ((ret = StringNew(string)) == nullptr)
            return nullptr;

        if (!DictInsert(intern, (ArObject *) ret, (ArObject *) ret)) {
            Release(ret);
            return nullptr;
        }

        ret->intern = true;
    }

    return ret;
}

String *argon::vm::datatype::StringNew(const char *string, ArSize length) {
    StringBuilder builder;
    Error *error;
    String *str;

    builder.Write((const unsigned char *) string, length, 0);

    if ((str = builder.BuildString()) == nullptr) {
        if ((error = builder.GetError()) != nullptr)
            argon::vm::Panic((ArObject *) error);

        return nullptr;
    }

    return str;
}

String *argon::vm::datatype::StringNew(unsigned char *buffer, ArSize length, ArSize cp_length, StringKind kind) {
    auto *str = StringInit(length, false);

    if (str != nullptr) {
        str->buffer = buffer;
        buffer[length] = '\0';
        str->cp_length = cp_length;
        str->kind = kind;
    }

    return str;
}
