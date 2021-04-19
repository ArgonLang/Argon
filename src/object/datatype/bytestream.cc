// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <vm/runtime.h>

#include "bool.h"
#include "bounds.h"
#include "error.h"
#include "integer.h"
#include "iterator.h"
#include "bytestream.h"

using namespace argon::memory;
using namespace argon::object;

bool CheckSize(ByteStream *bs, ArSize count) {
    unsigned char *tmp;
    ArSize cap = count > 1 ? bs->cap + count : (bs->cap + 1) + ((bs->cap + 1) / 2);

    if (bs->len + count > bs->cap) {
        if (bs->buffer == nullptr)
            cap = ARGON_OBJECT_BYTESTREAM_INITIAL_CAP;

        if ((tmp = (unsigned char *) Realloc(bs->buffer, cap)) == nullptr) {
            argon::vm::Panic(error_out_of_memory);
            return false;
        }

        bs->buffer = tmp;
        bs->cap = cap;
    }

    return true;
}

ArSize bytestream_len(ByteStream *self) {
    return self->len;
}

ArObject *bytestream_get_item(ByteStream *self, ArSSize index) {
    if (index < 0)
        index = self->len + index;

    if (index < self->len)
        return IntegerNew(self->buffer[index]);

    return ErrorFormat(&error_overflow_error, "bytestream index out of range (len: %d, idx: %d)", self->len, index);
}

bool bytestream_set_item(ByteStream *self, ArObject *obj, ArSSize index) {
    ArSize value;

    if (!AR_TYPEOF(obj, type_integer_)) {
        ErrorFormat(&error_type_error, "expected int found '%s'", AR_TYPE_NAME(obj));
        return false;
    }

    value = ((Integer *) obj)->integer;
    if (value < 0 || value > 255) {
        ErrorFormat(&error_value_error, "byte must be in range(0, 255)");
        return false;
    }

    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        self->buffer[index] = value;
        return true;
    }

    ErrorFormat(&error_overflow_error, "bytestream index out of range (len: %d, idx: %d)", self->len, index);
    return false;
}

ArObject *bytestream_get_slice(ByteStream *self, Bounds *bounds) {
    ByteStream *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, self->len, &start, &stop, &step);

    if ((ret = ByteStreamNew(slice_len, true, false)) == nullptr)
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

const SequenceSlots bytestream_sequence = {
        (SizeTUnaryOp) bytestream_len,
        (BinaryOpArSize) bytestream_get_item,
        (BoolTernOpArSize) bytestream_set_item,
        (BinaryOp) bytestream_get_slice,
        nullptr
};

ArObject *bytestream_add(ByteStream *self, ArObject *other) {
    ArBuffer buffer = {};
    ByteStream *ret;

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((ret = ByteStreamNew(self->len + buffer.len, true, false)) == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(ret->buffer, self->buffer, self->len);
    MemoryCopy(ret->buffer + self->len, buffer.buffer, buffer.len);

    BufferRelease(&buffer);
    return ret;
}

ArObject *bytestream_mul(ArObject *left, ArObject *right) {
    auto *bytes = (ByteStream *) left;
    auto *num = (Integer *) right;
    ByteStream *ret = nullptr;

    ArSize len;

    if (!AR_TYPEOF(bytes, type_bytestream_)) {
        bytes = (ByteStream *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        len = bytes->len * num->integer;

        if ((ret = ByteStreamNew(len, true, false)) != nullptr) {
            for (size_t i = 0; i < num->integer; i++)
                MemoryCopy(ret->buffer + bytes->len * i, bytes->buffer, bytes->len);
        }
    }

    return ret;
}

ByteStream *ShiftBytestream(ByteStream *bytes, ArSSize pos) {
    auto ret = ByteStreamNew(bytes->len, true, false);

    if (ret != nullptr) {
        for (size_t i = 0; i < bytes->len; i++)
            ret->buffer[((bytes->len + pos) + i) % bytes->len] = bytes->buffer[i];
    }

    return ret;
}

ArObject *bytestream_shl(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_bytestream_) && AR_TYPEOF(right, type_integer_))
        return ShiftBytestream((ByteStream *) left, -((Integer *) right)->integer);

    return nullptr;
}

ArObject *bytestream_shr(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_bytestream_) && AR_TYPEOF(right, type_integer_))
        return ShiftBytestream((ByteStream *) left, ((Integer *) right)->integer);

    return nullptr;
}

ArObject *bytestream_iadd(ByteStream *self, ArObject *other) {
    ArBuffer buffer = {};

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if (!CheckSize(self, buffer.len)) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(self->buffer + self->len, buffer.buffer, buffer.len);
    self->len += buffer.len;

    BufferRelease(&buffer);
    return IncRef(self);
}

OpSlots bytestream_ops{
        (BinaryOp) bytestream_add,
        nullptr,
        bytestream_mul,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        bytestream_shl,
        bytestream_shr,
        nullptr,
        (BinaryOp) bytestream_iadd,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *bytestream_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("bytestream", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (AR_TYPEOF(*args, type_integer_))
            return ByteStreamNew(((Integer *) *args)->integer, true, true);

        return ByteStreamNew(*args);
    }

    return ByteStreamNew();
}

ArObject *bytestream_str(ByteStream *self) {
    StringBuilder sb = {};

    // Calculate length of string
    if (!StringBuilderResizeAscii(&sb, self->buffer, self->len, 10 + 4)) // Bytestream + 2"" +2()
        return nullptr;

    // Build string
    StringBuilderWrite(&sb, (const unsigned char *) "ByteStream(", 11);
    StringBuilderWrite(&sb, (const unsigned char *) "\"", 1);
    StringBuilderWriteAscii(&sb, self->buffer, self->len);
    StringBuilderWrite(&sb, (const unsigned char *) "\")", 2);

    return StringBuilderFinish(&sb);
}

ArObject *bytestream_iter_get(ByteStream *self) {
    return IteratorNew(self, false);
}

ArObject *bytestream_iter_rget(ByteStream *self) {
    return IteratorNew(self, true);
}

ArObject *bytestream_compare(ByteStream *self, ArObject *other, CompareMode mode) {
    auto *o = (ByteStream *) other;
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
        else if (self->len < o->len)
            left = -1;
        else if (self->len > o->len)
            right = -1;
    }

    ARGON_RICH_COMPARE_CASES(left, right, mode)
}

bool bytestream_is_true(ByteStream *self) {
    return self->len > 0;
}

void bytestream_cleanup(ByteStream *self) {
    Free(self->buffer);
}

const TypeInfo argon::object::type_bytestream_ = {
        TYPEINFO_STATIC_INIT,
        "bytestream",
        nullptr,
        sizeof(ByteStream),
        TypeInfoFlags::BASE,
        bytestream_ctor,
        (VoidUnaryOp) bytestream_cleanup,
        nullptr,
        (CompareOp) bytestream_compare,
        (BoolUnaryOp) bytestream_is_true,
        nullptr,
        (UnaryOp) bytestream_str,
        (UnaryOp) bytestream_iter_get,
        (UnaryOp) bytestream_iter_rget,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &bytestream_sequence,
        &bytestream_ops,
        nullptr,
        nullptr
};

ByteStream *argon::object::ByteStreamNew(ArObject *object) {
    ArBuffer buffer = {};
    ByteStream *bs;

    if (!IsBufferable(object))
        return nullptr;

    if (!BufferGet(object, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((bs = ByteStreamNew(buffer.len, true, false)) != nullptr)
        MemoryCopy(bs->buffer, buffer.buffer, buffer.len);

    BufferRelease(&buffer);

    return bs;
}

ByteStream *argon::object::ByteStreamNew(ArSize cap, bool same_len, bool fill_zero) {
    auto bs = ArObjectNew<ByteStream>(RCType::INLINE, &type_bytestream_);

    if (bs != nullptr) {
        bs->buffer = nullptr;

        if (cap > 0) {
            if ((bs->buffer = (unsigned char *) Alloc(cap)) == nullptr) {
                Release(bs);
                return (ByteStream *) argon::vm::Panic(error_out_of_memory);
            }

            if (fill_zero)
                MemoryZero(bs->buffer, cap);
        }

        bs->cap = cap;
        bs->len = same_len ? cap : 0;
    }

    return bs;
}
