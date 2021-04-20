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

#define BUFFER_GET(bs)              (bs->view.buffer)
#define BUFFER_LEN(bs)              (bs->view.len)
#define BUFFER_MAXLEN(left, right)  (BUFFER_LEN(left) > BUFFER_LEN(right) ? BUFFER_LEN(right) : BUFFER_LEN(self))

using namespace argon::memory;
using namespace argon::object;

ArSize bytestream_len(ByteStream *self) {
    return BUFFER_LEN(self);
}

ArObject *bytestream_get_item(ByteStream *self, ArSSize index) {
    if (index < 0)
        index = BUFFER_LEN(self) + index;

    if (index < BUFFER_LEN(self))
        return IntegerNew(BUFFER_GET(self)[index]);

    return ErrorFormat(type_overflow_error_, "bytestream index out of range (len: %d, idx: %d)",
                       BUFFER_LEN(self), index);
}

bool bytestream_set_item(ByteStream *self, ArObject *obj, ArSSize index) {
    ArSize value;

    if (!AR_TYPEOF(obj, type_integer_)) {
        ErrorFormat(type_type_error_, "expected int found '%s'", AR_TYPE_NAME(obj));
        return false;
    }

    value = ((Integer *) obj)->integer;
    if (value < 0 || value > 255) {
        ErrorFormat(type_value_error_, "byte must be in range(0, 255)");
        return false;
    }

    if (index < 0)
        index = BUFFER_LEN(self) + index;

    if (index < BUFFER_LEN(self)) {
        BUFFER_GET(self)[index] = value;
        return true;
    }

    ErrorFormat(type_overflow_error_, "bytestream index out of range (len: %d, idx: %d)", BUFFER_LEN(self), index);
    return false;
}

ArObject *bytestream_get_slice(ByteStream *self, Bounds *bounds) {
    ByteStream *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, BUFFER_LEN(self), &start, &stop, &step);

    if (step >= 0) {
        ret = ByteStreamNew(self, start, slice_len);
    } else {
        if ((ret = ByteStreamNew(slice_len, true, false)) == nullptr)
            return nullptr;

        for (size_t i = 0; stop < start; start += step)
            BUFFER_GET(ret)[i++] = BUFFER_GET(self)[start];
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

    if ((ret = ByteStreamNew(BUFFER_LEN(self) + buffer.len, true, false)) == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(BUFFER_GET(ret), BUFFER_GET(self), BUFFER_LEN(self));
    MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(self), buffer.buffer, buffer.len);

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
        len = BUFFER_LEN(bytes) * num->integer;

        if ((ret = ByteStreamNew(len, true, false)) != nullptr) {
            for (size_t i = 0; i < num->integer; i++)
                MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(bytes) * i, BUFFER_GET(bytes), BUFFER_LEN(bytes));
        }
    }

    return ret;
}

ByteStream *ShiftBytestream(ByteStream *bytes, ArSSize pos) {
    auto ret = ByteStreamNew(BUFFER_LEN(bytes), true, false);

    if (ret != nullptr) {
        for (size_t i = 0; i < BUFFER_LEN(bytes); i++)
            ret->view.buffer[((BUFFER_LEN(bytes) + pos) + i) % BUFFER_LEN(bytes)] = BUFFER_GET(bytes)[i];
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

    if (!BufferViewEnlarge(&self->view, buffer.len)) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(BUFFER_GET(self) + BUFFER_LEN(self), buffer.buffer, buffer.len);
    self->view.len += buffer.len;

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

ArObject *bytestream_ctor(const TypeInfo *type, ArObject **args, ArSize count) {
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
    if (!StringBuilderResizeAscii(&sb, BUFFER_GET(self), BUFFER_LEN(self), 10 + 4)) // Bytestream + 2"" +2()
        return nullptr;

    // Build string
    StringBuilderWrite(&sb, (const unsigned char *) "ByteStream(", 11);
    StringBuilderWrite(&sb, (const unsigned char *) "\"", 1);
    StringBuilderWriteAscii(&sb, BUFFER_GET(self), BUFFER_LEN(self));
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
        res = MemoryCompare(BUFFER_GET(self), BUFFER_GET(o), BUFFER_MAXLEN(self, o));
        if (res < 0)
            left = -1;
        else if (res > 0)
            right = -1;
        else if (BUFFER_LEN(self) < BUFFER_LEN(o))
            left = -1;
        else if (BUFFER_LEN(self) > BUFFER_LEN(o))
            right = -1;
    }

    ARGON_RICH_COMPARE_CASES(left, right, mode)
}

bool bytestream_is_true(ByteStream *self) {
    return BUFFER_LEN(self) > 0;
}

void bytestream_cleanup(ByteStream *self) {
    BufferViewDetach(&self->view);
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
        MemoryCopy(BUFFER_GET(bs), buffer.buffer, buffer.len);

    BufferRelease(&buffer);

    return bs;
}

ByteStream *argon::object::ByteStreamNew(ByteStream *stream, ArSize start, ArSize len) {
    auto bs = ArObjectNew<ByteStream>(RCType::INLINE, &type_bytestream_);

    if (bs != nullptr)
        BufferViewInit(&bs->view, &stream->view, start, len);

    return bs;
}

ByteStream *argon::object::ByteStreamNew(ArSize cap, bool same_len, bool fill_zero) {
    auto bs = ArObjectNew<ByteStream>(RCType::INLINE, &type_bytestream_);

    if (bs != nullptr) {
        if (!BufferViewInit(&bs->view, cap)) {
            Release(bs);
            return nullptr;
        }

        if (same_len)
            bs->view.len = cap;

        if (fill_zero)
            MemoryZero(BUFFER_GET(bs), cap);
    }

    return bs;
}

#undef BUFFER_GET
#undef BUFFER_LEN
#undef BUFFER_MAXLEN