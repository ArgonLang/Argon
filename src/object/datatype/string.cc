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
        str->cp_len = 0;
        str->hash = 0;
    }

    return str;
}

void FillBuffer(String *dst, size_t offset, const unsigned char *buf, size_t len) {
    StringKind kind = StringKind::ASCII;
    size_t idx = 0;
    size_t uidx = 0;

    while (idx < len) {
        dst->buffer[idx + offset] = buf[idx];

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

ArObject *string_add(ArObject *left, ArObject *right) {
    if (left->type == right->type && left->type == &type_string_)
        return StringConcat((String *) left, (String *) right);

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

        auto ret = StringInit(l->len * i->integer, true);

        if (ret != nullptr) {
            for (size_t times = 0; times < i->integer; times++)
                FillBuffer(ret, l->len * times, l->buffer, l->len);
        }

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
        TYPEINFO_STATIC_INIT,
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

String *argon::object::StringNew(const char *string, size_t len) {
    auto str = StringInit(len, true);

    if (str != nullptr && string != nullptr)
        FillBuffer(str, 0, (unsigned char *) string, len);

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

// Common Operations

bool argon::object::StringEq(String *string, const unsigned char *c_str, size_t len) {
    if (string->len != len)
        return false;

    for (size_t i = 0; i < string->len; i++) {
        if (string->buffer[i] != c_str[i])
            return false;
    }

    return true;
}

String *argon::object::StringConcat(String *left, String *right) {
    String *ret = StringInit(left->len + right->len, true);

    if (ret != nullptr) {
        memory::MemoryCopy(ret->buffer, left->buffer, left->len);
        memory::MemoryCopy(ret->buffer + left->len, right->buffer, right->len);

        ret->kind = left->kind;
        if (right->kind > left->kind)
            ret->kind = right->kind;
        ret->cp_len = left->cp_len + right->cp_len;
    }

    return ret;
}

String *argon::object::StringReplace(String *string, String *old, String *newval, arsize n) {
    String *nstring;

    size_t idx = 0;
    size_t nidx = 0;
    size_t newsz;

    if (string_equal(string, old) || n == 0) {
        IncRef(string);
        return string;
    }

    // Compute replacements
    n = support::Count(string->buffer, string->len, old->buffer, old->len, n);

    newsz = (string->len + n * (newval->len - old->len));

    // Allocate string
    if ((nstring = StringInit(newsz, true)) == nullptr)
        return nullptr;

    long match;
    while ((match = support::Find(string->buffer + idx, string->len - idx, old->buffer, old->len)) > -1) {
        FillBuffer(nstring, nidx, string->buffer + idx, match);

        idx += match + old->len;
        nidx += match;

        FillBuffer(nstring, nidx, newval->buffer, newval->len);
        nidx += newval->len;

        if (n > -1 && --n == 0)
            break;
    }
    FillBuffer(nstring, nidx, string->buffer + idx, string->len - idx);

    return nstring;
}

String *argon::object::StringSubs(String *string, size_t start, size_t end) {
    String *ret;

    if (start >= string->len || end >= string->len)
        return nullptr;

    if (end == 0)
        end = string->len;

    if ((ret = StringInit(end - start, true)) != nullptr)
        FillBuffer(ret, 0, string->buffer + start, end - start);

    return ret;
}

