// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bytes.h"
#include "error.h"
#include "hash_magic.h"
#include "boolean.h"
#include "integer.h"
#include "stringbuilder.h"
#include "vm/runtime.h"
#include "bounds.h"

#define BUFFER_GET(bs)              ((bs)->view.buffer)
#define BUFFER_LEN(bs)              ((bs)->view.length)
#define BUFFER_MAXLEN(left, right)  (BUFFER_LEN(left) > BUFFER_LEN(right) ? BUFFER_LEN(right) : BUFFER_LEN(self))
#define VIEW_GET(bs)                ((bs)->view)

#define SHARED_LOCK(bs)                         \
    do {                                        \
        if(!(bs)->frozen)                       \
            VIEW_GET(bs).ReadableBufferLock();  \
    } while(0)

#define SHARED_UNLOCK(bs)                       \
    do {                                        \
        if(!(bs)->frozen)                       \
            VIEW_GET(bs).ReadableRelease();     \
    } while(0)

using namespace argon::vm::datatype;

ArObject *bytes_get_item(Bytes *self, ArObject *index) {
    ArObject *ret = nullptr;
    IntegerUnderlying idx;

    if (!AR_TYPEOF(index, type_int_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(index));
        return nullptr;
    }

    idx = ((Integer *) index)->sint;

    SHARED_LOCK(self);

    if (idx < 0)
        idx = BUFFER_LEN(self) + idx;

    if (idx < BUFFER_LEN(self))
        ret = (ArObject *) UIntNew(BUFFER_GET(self)[idx]);
    else
        ErrorFormat(kOverflowError[0], "bytes index out of range (index: %d, length: %d)",
                    idx, BUFFER_LEN(self));

    SHARED_UNLOCK(self);

    return ret;
}

ArObject *bytes_get_slice(Bytes *self, Bounds *bounds) {
    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    Bytes *ret;

    if (!AR_TYPEOF(bounds, type_bounds_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(bounds));
        return nullptr;
    }

    SHARED_LOCK(self);

    slice_len = BoundsIndex(bounds, BUFFER_LEN(self), &start, &stop, &step);

    if (step < 0) {
        if ((ret = BytesNew(slice_len, true, false, self->frozen)) == nullptr)
            return nullptr;

        for (ArSize i = 0; stop < start; start += step)
            BUFFER_GET(ret)[i++] = BUFFER_GET(self)[start];
    } else
        ret = BytesNew(self, start, slice_len);

    SHARED_UNLOCK(self);

    return (ArObject *) ret;
}

ArObject *bytes_item_in(Bytes *self, ArObject *value) {
    auto *pattern = (Bytes *) value;
    ArSSize index;

    if (!AR_TYPEOF(value, type_bytes_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_bytes_->name, AR_TYPE_NAME(value));
        return nullptr;
    }

    if (self == pattern)
        return BoolToArBool(true);

    SHARED_LOCK(self);
    SHARED_LOCK(pattern);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), BUFFER_GET(pattern), BUFFER_LEN(pattern));

    SHARED_UNLOCK(pattern);
    SHARED_UNLOCK(self);

    return BoolToArBool(index >= 0);
}

ArSize bytes_length(const Bytes *self) {
    return BUFFER_LEN(self);
}

bool bytes_set_item(Bytes *self, ArObject *index, ArObject *value) {
    IntegerUnderlying idx;
    ArSize rvalue;

    if (self->frozen) {
        ErrorFormat(kTypeError[0], "unable to set item to frozen bytes object");
        return false;
    }

    if (!AR_TYPEOF(index, type_int_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(index));
        return false;
    }

    idx = ((Integer *) index)->sint;

    if (AR_TYPEOF(value, type_uint_))
        rvalue = ((Integer *) value)->uint;
    else if (AR_TYPEOF(value, type_bytes_)) {
        auto *other = (Bytes *) value;

        SHARED_LOCK(other);

        if (BUFFER_LEN(other) > 1) {
            ErrorFormat(kValueError[0], "expected %s of length 1 not %d", type_bytes_->name, BUFFER_LEN(other));

            SHARED_UNLOCK(other);

            return false;
        }

        rvalue = BUFFER_GET(other)[0];

        SHARED_UNLOCK(other);
    } else {
        ErrorFormat(kTypeError[0], "expected %s or %s, found '%s'", type_uint_->name, type_bytes_->name,
                    AR_TYPE_NAME(index));

        return false;
    }

    if (rvalue > 255) {
        ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

        return false;
    }

    VIEW_GET(self).WritableBufferLock();

    if (idx < 0)
        idx = BUFFER_LEN(self) + idx;

    if (idx < BUFFER_LEN(self))
        BUFFER_GET(self)[idx] = (unsigned char) rvalue;
    else
        ErrorFormat(kOverflowError[0], "bytes index out of range (index: %d, length: %d)",
                    idx, BUFFER_LEN(self));

    VIEW_GET(self).WritableRelease();

    return true;
}

const SubscriptSlots bytes_subscript = {
        (ArSize_UnaryOp) bytes_length,
        (BinaryOp) bytes_get_item,
        (Bool_TernaryOp) bytes_set_item,
        (BinaryOp) bytes_get_slice,
        nullptr,
        (BinaryOp) bytes_item_in
};

bool bytes_get_buffer(Bytes *self, ArBuffer *buffer, BufferFlags flags) {
    bool ok;

    if (flags == BufferFlags::READ)
        VIEW_GET(self).ReadableBufferLock();
    else
        VIEW_GET(self).WritableBufferLock();

    ok = BufferSimpleFill((ArObject *) self, buffer, flags, BUFFER_GET(self), 1, BUFFER_LEN(self), !self->frozen);

    if (!ok) {
        if (flags == BufferFlags::READ)
            VIEW_GET(self).ReadableRelease();
        else
            VIEW_GET(self).WritableRelease();
    }

    return ok;
}

void bytes_rel_buffer(ArBuffer *buffer) {
    auto *self = (Bytes *) buffer->object;

    if (buffer->flags == BufferFlags::READ)
        VIEW_GET(self).ReadableRelease();
    else
        VIEW_GET(self).WritableRelease();
}

const BufferSlots bytes_buffer = {
        (BufferGetFn) bytes_get_buffer,
        bytes_rel_buffer
};

ArObject *bytes_compare(Bytes *self, ArObject *other, CompareMode mode) {
    auto *o = (Bytes *) other;

    int left = 0;
    int right = 0;
    int res;

    if (self == o)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, o))
        return nullptr;

    SHARED_LOCK(self);
    SHARED_LOCK(o);

    if (BUFFER_LEN(self) < BUFFER_LEN(o))
        left = -1;
    else if (BUFFER_LEN(self) > BUFFER_LEN(o))
        right = -1;
    else {
        res = stratum::util::MemoryCompare(BUFFER_GET(self), BUFFER_GET(o), BUFFER_LEN(self));
        if (res < 0)
            left = -1;
        else if (res > 0)
            right = -1;
    }

    SHARED_UNLOCK(self);
    SHARED_UNLOCK(o);

    ARGON_RICH_COMPARE_CASES(left, right, mode)
}

ArObject *bytes_repr(Bytes *self) {
    StringBuilder builder{};
    String *ret;

    SHARED_LOCK(self);

    builder.Write((const unsigned char *) "b\"", 2, BUFFER_LEN(self) + 1);
    builder.WriteEscaped(BUFFER_GET(self), BUFFER_LEN(self), 1);
    builder.Write((const unsigned char *) "\"", 1, 0);

    SHARED_UNLOCK(self);

    if ((ret = builder.BuildString()) == nullptr) {
        argon::vm::Panic((ArObject *) builder.GetError());
        return nullptr;
    }

    return (ArObject *) ret;
}

ArSize bytes_hash(Bytes *self) {
    if (!self->frozen) {
        ErrorFormat(kUnhashableError[0], "unable to hash unfrozen bytes object");
        return 0;
    }

    if (self->hash == 0)
        self->hash = HashBytes(BUFFER_GET(self), BUFFER_LEN(self));

    return self->hash;
}

bool bytes_dtor(Bytes *self) {
    BufferViewDetach(&self->view);

    return true;
}

bool bytes_is_true(const Bytes *self) {
    return BUFFER_LEN(self) > 0;
}

TypeInfo BytesType = {
        AROBJ_HEAD_INIT_TYPE,
        "Bytes",
        nullptr,
        nullptr,
        sizeof(Bytes),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) bytes_dtor,
        nullptr,
        (ArSize_UnaryOp) bytes_hash,
        (Bool_UnaryOp) bytes_is_true,
        (CompareOp) bytes_compare,
        (UnaryConstOp) bytes_repr,
        nullptr,
        nullptr,
        nullptr,
        &bytes_buffer,
        nullptr,
        nullptr,
        &bytes_subscript,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_bytes_ = &BytesType;

Bytes *argon::vm::datatype::BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen) {
    auto *bs = MakeObject<Bytes>(&BytesType);

    if (bs != nullptr) {
        if (!BufferViewInit(&bs->view, cap)) {
            Release(bs);
            return nullptr;
        }

        if (same_len)
            bs->view.length = cap;

        if (fill_zero)
            memory::MemoryZero(BUFFER_GET(bs), cap);

        bs->hash = 0;
        bs->frozen = frozen;
    }

    return bs;
}

Bytes *argon::vm::datatype::BytesNew(const unsigned char *buffer, ArSize len, bool frozen) {
    auto *bs = BytesNew(len, true, false, frozen);

    if (bs != nullptr)
        memory::MemoryCopy(BUFFER_GET(bs), buffer, len);

    return bs;
}

Bytes *argon::vm::datatype::BytesNew(Bytes *bytes, ArSize start, ArSize length) {
    auto *bs = MakeObject<Bytes>(type_bytes_);

    if (bs != nullptr) {
        BufferViewInit(&bs->view, &bytes->view, start, length);

        bs->hash = 0;
        bs->frozen = bytes->frozen;
    }

    return bs;
}