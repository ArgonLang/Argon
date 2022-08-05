// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "dict.h"
#include "stringbuilder.h"
#include "arstring.h"

using namespace argon::vm::datatype;
using namespace argon::vm::memory;

#define STR_BUF(str) ((str)->buffer)
#define STR_LEN(str) ((str)->length)

static Dict *intern = nullptr;

String *StringInit(ArSize len, bool mkbuf) {
    auto str = MakeObject<String>(type_string_);

    if (str != nullptr) {
        str->buffer = nullptr;

        if (mkbuf) {
            // +1 is '\0'
            str->buffer = (unsigned char *) Alloc(len + 1);
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
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_string_ = &StringType;

String *argon::vm::datatype::StringFormat(const char *format, ...) {
    // TODO: STUB
    assert(false);
}

String *argon::vm::datatype::StringFormat(const char *format, va_list args) {
    assert(false);
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
