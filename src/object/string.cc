// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "string.h"
#include "hash_magic.h"

using namespace argon::object;

bool string_equal(ArObject *self, ArObject *other) {
    if (self != other) {
        if (self->type != other->type)
            return false;

        auto s = (String *) self;
        auto o = (String *) other;

        if (s->len != o->len)
            return false;

        for (size_t i = 0; i < s->len; i++) {
            if (s->buffer[i] != o->buffer[i])
                return false;
        }
    }
    return true;
}

size_t string_hash(ArObject *obj) {
    auto self = (String *) obj;
    if (self->hash == 0)
        self->hash = HashBytes(self->buffer, self->len);
    return self->hash;
}

bool string_istrue(String *self) {
    return self->len > 0;
}


void string_cleanup(ArObject *obj) {
    argon::memory::Free(((String *) obj)->buffer);
}

const TypeInfo type_string_ = {
        (const unsigned char *) "string",
        sizeof(String),
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) string_istrue,
        string_equal,
        string_hash,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        string_cleanup
};

String *argon::object::StringNew(const std::string &string) {
    auto str = (String *) argon::memory::Alloc(sizeof(String));
    assert(str != nullptr);
    str->strong_or_ref = 1;
    str->type = &type_string_;

    str->buffer = (unsigned char *) argon::memory::Alloc(string.length());
    assert(str->buffer != nullptr);

    auto c_tmp = string.c_str();
    for (size_t i = 0; i < string.length(); i++)
        str->buffer[i] = c_tmp[i];
    str->len = string.length();
    str->hash = 0;

    return str;
}
