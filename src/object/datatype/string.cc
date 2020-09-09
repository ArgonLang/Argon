// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0


#include "hash_magic.h"
#include "map.h"
#include "integer.h"
#include "string.h"

using namespace argon::object;

static Map *intern;

ArObject *string_add(ArObject *left, ArObject *right) {
    String *ret;

    if (left->type == right->type && left->type == &type_string_) {
        auto l = (String *) left;
        auto r = (String *) right;

        if ((ret = StringNew(nullptr, 0)) != nullptr) {
            ret->buffer = (unsigned char *) argon::memory::MemoryConcat(l->buffer, l->len, r->buffer, r->len);

            if (ret->buffer == nullptr) {
                argon::vm::Panic(OutOfMemoryError);
                Release(ret);
                ret = nullptr;
            }

            return ret;
        }
    }

    return nullptr;
}

ArObject *string_mul(ArObject *left, ArObject *right) {
    auto l = (String *) left;

    if (left->type != &type_string_) {
        l = (String *) right;
        right = left;
    }

    if (right->type == &type_integer_) {
        auto i = (Integer *) right;

        auto ret = StringNew(nullptr, l->len * i->integer);

        if (ret != nullptr)
            for (size_t times = 0; times < i->integer; times++)
                argon::memory::MemoryCopy(ret->buffer + (l->len * times), l->buffer, l->len);

        return ret;
    }

    return nullptr;
}

OpSlots string_ops{
        string_add,
        nullptr,
        string_mul,
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
        nullptr,
};

bool string_equal(ArObject *self, ArObject *other) {
    if (self == other)
        return true;

    if (self->type == other->type) {
        auto s = (String *) self;
        auto o = (String *) other;

        if (s->len != o->len)
            return false;

        for (size_t i = 0; i < s->len; i++) {
            if (s->buffer[i] != o->buffer[i])
                return false;
        }

        return true;
    }

    return false;
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
        &string_ops,
        nullptr,
        string_cleanup
};

String *argon::object::StringNew(const char *string, size_t len) {
    auto str = ArObjectNew<String>(RCType::INLINE, &type_string_);

    if (str != nullptr) {

        str->buffer = (unsigned char *) argon::memory::Alloc(len);
        if (str->buffer == nullptr) {
            Release(str);
            return (String *) argon::vm::Panic(OutOfMemoryError);
        }

        memory::MemoryCopy(str->buffer, string, len);

        str->len = len;
        str->hash = 0;
    }

    return str;
}

String *argon::object::StringIntern(const std::string &string) {
    String *ret = nullptr;

    if (intern == nullptr) {
        if ((intern = MapNew()) == nullptr)
            return nullptr;
    } else
        ret = (String *) MapGetFrmStr(intern, string.c_str(), string.size());

    if (ret == nullptr) {
        if ((ret = StringNew(string)) != nullptr) {
            if (!MapInsert(intern, ret, ret)) {
                ret = nullptr;
                Release(ret);
            }
        }
    }

    return ret;
}
