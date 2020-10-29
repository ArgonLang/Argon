// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/objmgmt.h>
#include "hash_magic.h"
#include "map.h"
#include "integer.h"

#include "string.h"

using namespace argon::memory;
using namespace argon::object;

static Map *intern;

ArObject *string_add(ArObject *left, ArObject *right) {
    String *ret;

    if (left->type == right->type && left->type == &type_string_) {
        auto l = (String *) left;
        auto r = (String *) right;

        if ((ret = StringNew(nullptr, l->len + r->len)) != nullptr) {
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

String *string_str(String *self) {
    IncRef(self);
    return self;
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
        (UnaryOp) string_str,
        &string_ops,
        nullptr,
        string_cleanup
};

String *StringInit(size_t len, bool mkbuf) {
    auto str = ArObjectNew<String>(RCType::INLINE, &type_string_);

    if (str != nullptr) {
        str->buffer = nullptr;

        if (mkbuf) {
            // +1 is '\0'
            if ((str->buffer = (unsigned char *) Alloc(len + 1)) == nullptr) {
                argon::vm::Panic(OutOfMemoryError);
                Release(str);
                return nullptr;
            }

            // Set terminator
            str->buffer[(len + 1) - 1] = 0x00;
        }

        str->kind = StringKind::ASCII;
        str->intern = false;
        str->len = len;
        str->cp_len = len;
        str->hash = 0;
    }

    return str;
}

void FillBuffer(String *dst, const unsigned char *buf, size_t len) {
    StringKind kind = StringKind::ASCII;
    size_t idx = 0;
    size_t uidx = 0;

    dst->cp_len = 0;

    while (idx < len) {
        dst->buffer[idx] = buf[idx];

        if (buf[idx] >> (unsigned char) 7 == 0x0)
            uidx += 1;
        else if (buf[idx] >> (unsigned char) 5 == 0x6) {
            kind = StringKind::UTF8_2;
            uidx += 2;
        } else if (buf[idx] >> (unsigned char) 4 == 0xE) {
            kind = StringKind::UTF8_3;
            uidx += 3;
        } else if (buf[idx] >> (unsigned char) 3 == 0x1E) {
            kind = StringKind::UTF8_4;
            uidx += 4;
        }

        if (++idx == uidx)
            dst->cp_len++;

        if (kind > dst->kind)
            dst->kind = kind;
    }
}

String *argon::object::StringNew(const char *string, size_t len) {
    auto str = StringInit(len, true);

    if (str != nullptr && string != nullptr)
        FillBuffer(str, (unsigned char *) string, len);

    return str;
}

String *argon::object::StringIntern(const char *string, size_t len) {
    String *ret = nullptr;

    if (intern == nullptr) {
        intern = MapNew();
        // TODO: log!
    } else
        ret = (String *) MapGetFrmStr(intern, string, len);

    if (ret == nullptr) {
        if ((ret = StringNew(string, len)) != nullptr) {
            if (intern != nullptr) {
                if (MapInsert(intern, ret, ret))
                    ret->intern = true;
                // TODO: log if false!
            }
        }
    }

    return ret;
}
