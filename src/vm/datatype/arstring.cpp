// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"

using namespace argon::vm::datatype;
using namespace argon::vm::memory;

#define STR_BUF(str) ((str)->buffer)
#define STR_LEN(str) ((str)->length)

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

String *argon::vm::datatype::StringNew(const char *string, ArSize length) {
    // TODO: STUB
    assert(false);
}

String *argon::vm::datatype::StringFormat(const char *format, ...) {
    // TODO: STUB
    assert(false);
}