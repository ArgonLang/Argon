// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/memory/memory.h>
#include <vm/runtime.h>

#include "boolean.h"
#include "bounds.h"
#include "dict.h"
#include "hash_magic.h"
#include "integer.h"
#include "stringbuilder.h"
#include "arstring.h"

using namespace argon::vm::datatype;

#define STR_BUF(str) ((str)->buffer)
#define STR_LEN(str) ((str)->length)

static Dict *intern = nullptr;
static String *empty_string = nullptr;

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

bool string_get_buffer(String *self, ArBuffer *buffer, BufferFlags flags) {
    return BufferSimpleFill((ArObject *) self, buffer, flags, self->buffer, 1, self->length, false);
}

const BufferSlots string_buffer = {
        (BufferGetFn) string_get_buffer,
        nullptr
};

const FunctionDef string_methods[] = {
        ARGON_METHOD_SENTINEL
};

const ObjectSlots string_objslot = {
        string_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArSize string_length(const String *self) {
    return STR_LEN(self);
}

ArObject *string_get_item(const String *self, ArObject *index) {
    ArSSize idx;

    if (self->kind != StringKind::ASCII) {
        ErrorFormat(kUnicodeError[0], kUnicodeError[2]);
        return nullptr;
    }

    if (AR_TYPEOF(index, type_int_)) {
        idx = ((Integer *) index)->sint;
        if (idx < 0)
            idx = (ArSSize) (STR_LEN(self) + idx);
    } else {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(index));
        return nullptr;
    }

    return (ArObject *) StringIntern((const char *) STR_BUF(self) + idx, 1);
}

ArObject *string_get_slice(const String *self, ArObject *bounds) {
    auto *b = (Bounds *) bounds;
    String *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    if (self->kind != StringKind::ASCII) {
        ErrorFormat(kUnicodeError[0], kUnicodeError[3]);
        return nullptr;
    }

    slice_len = BoundsIndex(b, STR_LEN(self), &start, &stop, &step);

    if ((ret = StringInit(slice_len, true)) == nullptr)
        return nullptr;

    ret->cp_length = slice_len;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step)
            ret->buffer[i++] = self->buffer[start];
    } else {
        for (ArSize i = 0; stop < start; start += step)
            ret->buffer[i++] = self->buffer[start];
    }

    return (ArObject *) ret;
}

ArObject *string_in(const String *self, const String *pattern) {
    ArSSize index = StringFind(self, pattern);

    return BoolToArBool(index >= 0);
}

const SubscriptSlots string_subscript = {
        (ArSize_UnaryOp) string_length,
        (BinaryOp) string_get_item,
        nullptr,
        (BinaryOp) string_get_slice,
        nullptr,
        (BinaryOp) string_in
};

ArObject *string_add(const String *left, const String *right) {
    if (AR_TYPEOF(left, type_string_) && AR_SAME_TYPE(left, right))
        return (ArObject *) StringConcat(left, right);

    return nullptr;
}

ArObject *string_mul(const String *left, const ArObject *right) {
    const auto *l = left;
    String *ret = nullptr;
    UIntegerUnderlying times;

    // int * str -> str * int
    if (!AR_TYPEOF(left, type_string_)) {
        l = (const String *) right;
        right = (const ArObject *) left;
    }

    if (AR_TYPEOF(right, type_uint_)) {
        times = ((const Integer *) right)->uint;
        if ((ret = StringInit(l->length * times, true)) != nullptr) {
            ret->cp_length = l->cp_length * times;

            while (times--)
                argon::vm::memory::MemoryCopy(ret->buffer + l->length * times, l->buffer, l->length);

            ret->kind = l->kind;
        }
    }

    return (ArObject *) ret;
}

const OpSlots string_ops = {
        (BinaryOp) string_add,
        nullptr,
        (BinaryOp) string_mul,
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
        (BinaryOp) string_add,
        nullptr,
        nullptr,
        nullptr
};

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

ArObject *string_iter(String *self, bool reverse) {
    auto *si = MakeObject<StringIterator>(type_string_iterator_);

    if (si != nullptr) {
        si->iterable = IncRef(self);
        si->index = 0;
        si->reverse = reverse;
    }

    return (ArObject *) si;
}

ArObject *string_str(String *self) {
    return (ArObject *) IncRef(self);
}

ArObject *string_repr(const String *self) {
    StringBuilder builder;

    builder.Write((const unsigned char *) "\"", 1, STR_LEN(self) + 1);
    builder.WriteEscaped(STR_BUF(self), STR_LEN(self), 1, true);
    builder.Write((const unsigned char *) "\"", 1, 0);

    auto *ret = (ArObject *) builder.BuildString();
    if (ret == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(ret);

        return nullptr;
    }

    return ret;
}

ArSize string_hash(String *self) {
    if (self->hash == 0)
        self->hash = HashBytes(STR_BUF(self), STR_LEN(self));

    return self->hash;
}

bool string_dtor(String *self) {
    argon::vm::memory::Free(STR_BUF(self));

    return true;
}

bool string_istrue(const String *self) {
    return STR_LEN(self) > 0;
}

TypeInfo StringType = {
        AROBJ_HEAD_INIT_TYPE,
        "String",
        nullptr,
        nullptr,
        sizeof(String),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) string_dtor,
        nullptr,
        (ArSize_UnaryOp) string_hash,
        (Bool_UnaryOp) string_istrue,
        (CompareOp) string_compare,
        (UnaryConstOp) string_repr,
        (UnaryOp) string_str,
        (UnaryBoolOp) string_iter,
        nullptr,
        &string_buffer,
        nullptr,
        &string_objslot,
        &string_subscript,
        &string_ops,
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
    auto *astr = StringNew(string, length);
    if (astr == nullptr)
        return nullptr;

    auto *concat = StringConcat(left, astr);

    Release(astr);

    return concat;
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

String *argon::vm::datatype::StringIntern(const char *string, ArSize length) {
    String *ret;

    // Initialize intern
    if (intern == nullptr) {
        intern = DictNew();
        if (intern == nullptr)
            return nullptr;

        // Insert empty string
        if ((ret = StringInit(0, true)) == nullptr)
            return nullptr;

        if (!DictInsert(intern, (ArObject *) ret, (ArObject *) ret)) {
            Release(ret);
            return nullptr;
        }

        ret->intern = true;

        empty_string = ret;

        if (string == nullptr || length == 0)
            return ret;

        Release(ret);
    }

    if (string == nullptr || length == 0)
        return IncRef(empty_string);

    if ((ret = (String *) DictLookup(intern, string)) == nullptr) {
        if ((ret = StringNew(string, length)) == nullptr)
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

// STRING ITERATOR

ArObject *stringiterator_iter_next(StringIterator *self) {
    const unsigned char *buf = STR_BUF(self->iterable) + self->index;
    String *ret;
    ArSize len = 1;

    if (self->iterable == nullptr)
        return nullptr;

    if (!self->reverse)
        len = StringSubstrLen(self->iterable, self->index, 1);
    else {
        buf--;

        while (buf > STR_BUF(self->iterable) && (*buf >> 6 == 0x2)) {
            buf--;
            len++;
        }
    }

    if ((ret = StringIntern((const char *) buf, len)) != nullptr) {
        if (self->reverse)
            self->index -= len;
        else
            self->index += len;
    }

    return (ArObject *) ret;
}

TypeInfo StringIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "StringIterator",
        nullptr,
        nullptr,
        sizeof(StringIterator),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        IteratorIter,
        (UnaryOp) stringiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_string_iterator_ = &StringIteratorType;