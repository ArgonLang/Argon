// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "bool.h"
#include "error.h"
#include "integer.h"
#include "iterator.h"
#include "string.h"
#include "hash_magic.h"
#include "bytes.h"
#include "bounds.h"

using namespace argon::object;
using namespace argon::memory;

bool bytes_get_buffer(Bytes *self, ArBuffer *buffer, ArBufferFlags flags) {
    return BufferSimpleFill(self, buffer, flags, self->buffer, self->len, false);
}

const BufferSlots bytes_buffer = {
        (BufferGetFn) bytes_get_buffer,
        nullptr
};

ArSize bytes_len(Bytes *self) {
    return self->len;
}

ArObject *bytes_get_item(Bytes *self, ArSSize index) {
    if (index < 0)
        index = self->len + index;

    if (index < self->len)
        return IntegerNew(self->buffer[index]);

    return ErrorFormat(&error_overflow_error, "bytes index out of range (len: %d, idx: %d)", self->len, index);
}

ArObject *bytes_get_slice(Bytes *self, Bounds *bounds) {
    Bytes *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, self->len, &start, &stop, &step);

    if ((ret = BytesNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (size_t i = 0; start < stop; start += step)
            ret->buffer[i++] = self->buffer[start];
    } else {
        for (size_t i = 0; stop < start; start += step)
            ret->buffer[i++] = self->buffer[start];
    }

    return ret;
}

const SequenceSlots bytes_sequence = {
        (SizeTUnaryOp) bytes_len,
        (BinaryOpArSize) bytes_get_item,
        nullptr,
        (BinaryOp) bytes_get_slice
};

ArObject *bytes_add(Bytes *self, ArObject *other) {
    auto *o = (Bytes *) other;
    Bytes *ret;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if ((ret = BytesNew(self->len + o->len)) == nullptr)
        return nullptr;

    MemoryCopy(ret->buffer, self->buffer, self->len);
    MemoryCopy(ret->buffer + self->len, o->buffer, o->len);

    return ret;
}

ArObject *bytes_mul(ArObject *left, ArObject *right) {
    auto *bytes = (Bytes *) left;
    auto *num = (Integer *) right;
    Bytes *ret = nullptr;

    size_t len;

    if (!AR_TYPEOF(bytes, type_bytes_)) {
        bytes = (Bytes *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        len = bytes->len * num->integer;

        if ((ret = BytesNew(len)) != nullptr) {
            for (size_t i = 0; i < num->integer; i++)
                MemoryCopy(ret->buffer + bytes->len * i, bytes->buffer, bytes->len);
        }
    }

    return ret;
}

Bytes *ShiftBytes(Bytes *bytes, ArSSize pos) {
    auto ret = BytesNew(bytes->len);

    if (ret != nullptr) {
        for (size_t i = 0; i < bytes->len; i++)
            ret->buffer[((bytes->len + pos) + i) % bytes->len] = bytes->buffer[i];
    }

    return ret;
}

ArObject *bytes_shl(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_bytes_) && AR_TYPEOF(right, type_integer_))
        return ShiftBytes((Bytes *) left, -((Integer *) right)->integer);

    return nullptr;
}

ArObject *bytes_shr(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_bytes_) && AR_TYPEOF(right, type_integer_))
        return ShiftBytes((Bytes *) left, ((Integer *) right)->integer);

    return nullptr;
}

OpSlots bytes_ops{
        (BinaryOp) bytes_add,
        nullptr,
        bytes_mul,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        bytes_shl,
        bytes_shr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool bytes_is_true(Bytes *self) {
    return self->len > 0;
}

bool bytes_equal(Bytes *self, ArObject *other) {
    auto *o = (Bytes *) other;

    if (self == other)
        return true;

    if (!AR_SAME_TYPE(self, other) || self->len != o->len)
        return false;

    return MemoryCompare(self->buffer, o->buffer, self->len) == 0;
}

ArObject *bytes_ctor(ArObject **args, ArSize count) {
    IntegerUnderlying size = 0;

    if (!VariadicCheckPositional("bytes", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (!AR_TYPEOF(*args, type_integer_))
            return BytesNew(*args);
        size = ((Integer *) *args)->integer;
    }

    return BytesNew(size);
}

ArObject *bytes_compare(Bytes *self, ArObject *other, CompareMode mode) {
    auto *o = (Bytes *) other;
    int left = 0;
    int right = 0;
    int res;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self != other) {
        res = MemoryCompare(self->buffer, o->buffer, self->len > o->len ? o->len : self->len);
        if (res < 0)
            left = -1;
        else if (res > 0)
            right = -1;
    }

    switch (mode) {
        case CompareMode::GE:
            return BoolToArBool(left > right);
        case CompareMode::GEQ:
            return BoolToArBool(left >= right);
        case CompareMode::LE:
            return BoolToArBool(left < right);
        case CompareMode::LEQ:
            return BoolToArBool(left <= right);
        default:
            assert(false);
    }
}

size_t bytes_hash(Bytes *self) {
    if (self->hash == 0)
        self->hash = HashBytes(self->buffer, self->len);

    return self->hash;
}

ArObject *bytes_str(Bytes *self) {
    StringBuilder sb = {};

    // Calculate length of string
    if (!StringBuilderResizeAscii(&sb, self->buffer, self->len, 2))
        return nullptr;

    // Build string
    StringBuilderWrite(&sb, (const unsigned char *) "\"", 1);
    StringBuilderWriteAscii(&sb, self->buffer, self->len);
    StringBuilderWrite(&sb, (const unsigned char *) "\"", 1);

    return StringBuilderFinish(&sb);
}

ArObject *bytes_iter_get(Bytes *self) {
    return IteratorNew(self, false);
}

ArObject *bytes_iter_rget(Bytes *self) {
    return IteratorNew(self, true);
}

void bytes_cleanup(Bytes *self) {
    Free(self->buffer);
}

const TypeInfo argon::object::type_bytes_ = {
        TYPEINFO_STATIC_INIT,
        "bytes",
        nullptr,
        sizeof(Bytes),
        bytes_ctor,
        (VoidUnaryOp) bytes_cleanup,
        nullptr,
        (CompareOp) bytes_compare,
        (BoolBinOp) bytes_equal,
        (BoolUnaryOp) bytes_is_true,
        (SizeTUnaryOp) bytes_hash,
        (UnaryOp) bytes_str,
        (UnaryOp) bytes_iter_get,
        (UnaryOp) bytes_iter_rget,
        &bytes_buffer,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &bytes_sequence,
        &bytes_ops,
        nullptr
};

Bytes *argon::object::BytesNew(size_t len) {
    auto *bytes = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (bytes != nullptr) {
        bytes->buffer = nullptr;
        if (len > 0) {
            if ((bytes->buffer = (unsigned char *) Alloc(len)) == nullptr) {
                Release(bytes);
                return nullptr;
            }

            MemoryZero(bytes->buffer, len);
        }

        bytes->len = len;
    }

    return bytes;
}

Bytes *argon::object::BytesNew(ArObject *object) {
    ArBuffer buffer = {};
    Bytes *bytes;

    if (!IsBufferable(object))
        return nullptr;

    if (!BufferGet(object, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((bytes = BytesNew(buffer.len)) == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(bytes->buffer, buffer.buffer, buffer.len);

    BufferRelease(&buffer);
    return bytes;
}

Bytes *argon::object::BytesNew(const std::string &string) {
    auto bytes = BytesNew(string.length());

    if (bytes != nullptr)
        MemoryCopy(bytes->buffer, string.c_str(), string.length());

    return bytes;
}