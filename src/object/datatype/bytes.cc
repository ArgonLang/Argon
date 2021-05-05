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

#define BUFFER_GET(bs)              (bs->view.buffer)
#define BUFFER_LEN(bs)              (bs->view.len)
#define BUFFER_MAXLEN(left, right)  (BUFFER_LEN(left) > BUFFER_LEN(right) ? BUFFER_LEN(right) : BUFFER_LEN(self))

using namespace argon::object;
using namespace argon::memory;

bool bytes_get_buffer(Bytes *self, ArBuffer *buffer, ArBufferFlags flags) {
    return BufferSimpleFill(self, buffer, flags, BUFFER_GET(self), BUFFER_LEN(self), false);
}

const BufferSlots bytes_buffer = {
        (BufferGetFn) bytes_get_buffer,
        nullptr
};

ArSize bytes_len(Bytes *self) {
    return BUFFER_LEN(self);
}

ArObject *bytes_get_item(Bytes *self, ArSSize index) {
    if (index < 0)
        index = BUFFER_LEN(self) + index;

    if (index < BUFFER_LEN(self))
        return IntegerNew(BUFFER_GET(self)[index]);

    return ErrorFormat(type_overflow_error_, "bytes index out of range (len: %d, idx: %d)", BUFFER_LEN(self), index);
}

ArObject *bytes_get_slice(Bytes *self, Bounds *bounds) {
    Bytes *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, BUFFER_LEN(self), &start, &stop, &step);

    if (step >= 0) {
        ret = BytesNew(self, start, slice_len);
    } else {
        if ((ret = BytesNew(slice_len)) == nullptr)
            return nullptr;

        for (size_t i = 0; stop < start; start += step)
            BUFFER_GET(ret)[i++] = BUFFER_GET(self)[start];
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

    if ((ret = BytesNew(BUFFER_LEN(self) + BUFFER_LEN(o))) == nullptr)
        return nullptr;

    MemoryCopy(BUFFER_GET(ret), BUFFER_GET(self), BUFFER_LEN(self));
    MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(self), BUFFER_GET(o), BUFFER_LEN(o));

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
        len = BUFFER_LEN(bytes) * num->integer;

        if ((ret = BytesNew(len)) != nullptr) {
            for (size_t i = 0; i < num->integer; i++)
                MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(bytes) * i, BUFFER_GET(bytes), BUFFER_LEN(bytes));
        }
    }

    return ret;
}

Bytes *ShiftBytes(Bytes *bytes, ArSSize pos) {
    auto ret = BytesNew(BUFFER_LEN(bytes));

    if (ret != nullptr) {
        for (size_t i = 0; i < BUFFER_LEN(bytes); i++)
            ret->view.buffer[((BUFFER_LEN(bytes) + pos) + i) % BUFFER_LEN(bytes)] = BUFFER_GET(bytes)[i];
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

ARGON_FUNCTION5(bytes_, new, "Creates bytes object."
                             ""
                             "The src parameter is optional, in case of call without src parameter an empty zero-length"
                             "bytes object will be constructed."
                             ""
                             "- Parameter [src]: integer or bufferable object."
                             "- Returns: construct a new bytes object.", 0, true) {
    IntegerUnderlying size = 0;

    if (!VariadicCheckPositional("bytes::new", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (!AR_TYPEOF(*argv, type_integer_))
            return BytesNew(*argv);
        size = ((Integer *) *argv)->integer;
    }

    return BytesNew(size);
}

ARGON_METHOD5(bytes_, str, "Convert bytes to str object."
                           ""
                           "- Returns: new str object.", 0, false) {
    auto *bytes = (Bytes *) self;

    return StringNew((const char *) bytes->view.buffer, bytes->view.len);
}

const NativeFunc bytes_methods[] = {
        bytes_new_,
        bytes_str_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots bytes_obj = {
        bytes_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool bytes_is_true(Bytes *self) {
    return BUFFER_LEN(self) > 0;
}

ArObject *bytes_compare(Bytes *self, ArObject *other, CompareMode mode) {
    auto *o = (Bytes *) other;
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

size_t bytes_hash(Bytes *self) {
    if (self->hash == 0)
        self->hash = HashBytes(BUFFER_GET(self), BUFFER_LEN(self));

    return self->hash;
}

ArObject *bytes_str(Bytes *self) {
    StringBuilder sb = {};

    // Calculate length of string
    if (!StringBuilderResizeAscii(&sb, BUFFER_GET(self), BUFFER_LEN(self), 2))
        return nullptr;

    // Build string
    StringBuilderWrite(&sb, (const unsigned char *) "\"", 1);
    StringBuilderWriteAscii(&sb, BUFFER_GET(self), BUFFER_LEN(self));
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
    BufferViewDetach(&self->view);
}

const TypeInfo argon::object::type_bytes_ = {
        TYPEINFO_STATIC_INIT,
        "bytes",
        nullptr,
        sizeof(Bytes),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) bytes_cleanup,
        nullptr,
        (CompareOp) bytes_compare,
        (BoolUnaryOp) bytes_is_true,
        (SizeTUnaryOp) bytes_hash,
        (UnaryOp) bytes_str,
        (UnaryOp) bytes_iter_get,
        (UnaryOp) bytes_iter_rget,
        &bytes_buffer,
        nullptr,
        nullptr,
        nullptr,
        &bytes_obj,
        &bytes_sequence,
        &bytes_ops,
        nullptr,
        nullptr
};

Bytes *argon::object::BytesNew(ArSize len) {
    auto *bytes = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (bytes != nullptr) {
        if (!BufferViewInit(&bytes->view, len)) {
            Release(bytes);
            return nullptr;
        }

        MemoryZero(BUFFER_GET(bytes), len);
        bytes->view.len = len;
    }

    return bytes;
}

Bytes *argon::object::BytesNew(ArObject *object) {
    ArBuffer buffer = {};
    Bytes *bytes;

    if (!BufferGet(object, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((bytes = BytesNew(buffer.len)) == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(BUFFER_GET(bytes), buffer.buffer, buffer.len);

    BufferRelease(&buffer);
    return bytes;
}

Bytes *argon::object::BytesNew(Bytes *bytes, ArSize start, ArSize len) {
    auto ret = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (ret != nullptr)
        BufferViewInit(&ret->view, &bytes->view, start, len);

    return ret;
}

Bytes *argon::object::BytesNew(const std::string &string) {
    auto bytes = BytesNew(string.length());

    if (bytes != nullptr)
        MemoryCopy(BUFFER_GET(bytes), string.c_str(), string.length());

    return bytes;
}

#undef BUFFER_GET
#undef BUFFER_LEN
#undef BUFFER_MAXLEN