// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "string.h"
#include "hash_magic.h"
#include "map.h"

using namespace argon::object;

static Map *intern;

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

bool argon::object::StringEq(String *string, const unsigned char *c_str, size_t len) {
    if (string->len != len)
        return false;

    for (size_t i = 0; i < string->len; i++) {
        if (string->buffer[i] != c_str[i])
            return false;
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

const TypeInfo argon::object::type_string_ = {
        (const unsigned char *) "string",
        sizeof(String),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) string_istrue,
        string_equal,
        nullptr,
        string_hash,
        nullptr,
        string_cleanup
};

String *argon::object::StringNew(const char *string, size_t len) {
    auto str = (String *) argon::memory::Alloc(sizeof(String));
    assert(str != nullptr); // TODO: enomem

    str->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
    str->type = &type_string_;

    str->buffer = (unsigned char *) argon::memory::Alloc(len);
    assert(str->buffer != nullptr); // TODO: enomem

    memory::MemoryCopy(str->buffer, string, len);

    str->len = len;
    str->hash = 0;

    return str;
}

String *argon::object::StringIntern(const std::string &string) {
    String *ret = nullptr;

    if (intern == nullptr) {
        intern = MapNew();
        assert(intern != nullptr);
    } else
        ret = (String *) MapGetFrmStr(intern, string.c_str(), string.size());

    if (ret == nullptr) {
        ret = StringNew(string);

        if (ret != nullptr) {
            if (!MapInsert(intern, ret, ret)) {
                ret = nullptr;
                Release(ret);
            }
        }
    }

    return ret;
}
