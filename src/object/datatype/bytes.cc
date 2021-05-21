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
#include "hash_magic.h"

#include "bytes.h"

#define BUFFER_GET(bs)              (bs->view.buffer)
#define BUFFER_LEN(bs)              (bs->view.len)
#define BUFFER_MAXLEN(left, right)  (BUFFER_LEN(left) > BUFFER_LEN(right) ? BUFFER_LEN(right) : BUFFER_LEN(self))

using namespace argon::memory;
using namespace argon::object;

bool bytes_get_buffer(Bytes *self, ArBuffer *buffer, ArBufferFlags flags) {
    return BufferSimpleFill(self, buffer, flags, BUFFER_GET(self), BUFFER_LEN(self), !self->frozen);
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

    return ErrorFormat(type_overflow_error_, "bytes index out of range (len: %d, idx: %d)",
                       BUFFER_LEN(self), index);
}

bool bytes_set_item(Bytes *self, ArObject *obj, ArSSize index) {
    Bytes *other;
    ArSize value;

    if (self->frozen) {
        ErrorFormat(type_type_error_, "unable to set item to frozen bytes object");
        return false;
    }

    if (AR_TYPEOF(obj, type_bytes_)) {
        other = (Bytes *) obj;

        if (BUFFER_LEN(other) > 1) {
            ErrorFormat(type_value_error_, "expected bytes of length 1 not %d", BUFFER_LEN(other));
            return false;
        }

        value = BUFFER_GET(other)[0];
    } else if (AR_TYPEOF(obj, type_integer_))
        value = ((Integer *) obj)->integer;
    else {
        ErrorFormat(type_type_error_, "expected integer or bytes, found '%s'", AR_TYPE_NAME(obj));
        return false;
    }

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

    ErrorFormat(type_overflow_error_, "bytes index out of range (len: %d, idx: %d)", BUFFER_LEN(self), index);
    return false;
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
        if ((ret = BytesNew(slice_len, true, false, self->frozen)) == nullptr)
            return nullptr;

        for (ArSize i = 0; stop < start; start += step)
            BUFFER_GET(ret)[i++] = BUFFER_GET(self)[start];
    }

    return ret;
}

const SequenceSlots bytes_sequence = {
        (SizeTUnaryOp) bytes_len,
        (BinaryOpArSize) bytes_get_item,
        (BoolTernOpArSize) bytes_set_item,
        (BinaryOp) bytes_get_slice,
        nullptr
};

ARGON_FUNCTION5(bytes_, new, "Creates bytes object."
                             ""
                             "The src parameter is optional, in case of call without src parameter an empty zero-length"
                             "bytes object will be constructed."
                             ""
                             "- Parameter [src]: integer or bytes-like object."
                             "- Returns: construct a new bytes object.", 0, true) {
    IntegerUnderlying size = 0;

    if (!VariadicCheckPositional("bytes::new", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (!AR_TYPEOF(*argv, type_integer_))
            return BytesNew(*argv);

        size = ((Integer *) *argv)->integer;
    }

    return BytesNew(size, true, true, false);
}

ARGON_METHOD5(bytes_, count,
              "Returns the number of times a specified value occurs in bytes."
              ""
              "- Parameter sub: subsequence to search."
              "- Returns: number of times a specified value appears in bytes.", 1, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    ArSSize n;

    bytes = (Bytes *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    n = support::Count(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, -1);

    BufferRelease(&buffer);

    return IntegerNew(n);
}

ARGON_METHOD5(bytes_, clone,
              "Returns the number of times a specified value occurs in bytes."
              ""
              "- Parameter sub: subsequence to search."
              "- Returns: number of times a specified value appears in the string.", 0, false) {
    return BytesNew(self);
}

ARGON_METHOD5(bytes_, endswith,
              "Returns true if bytes ends with the specified value."
              ""
              "- Parameter suffix: the value to check if the bytes ends with."
              "- Returns: true if bytes ends with the specified value, false otherwise."
              ""
              "# SEE"
              "- startswith: Returns true if bytes starts with the specified value.", 1, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    int res;

    bytes = (Bytes *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    res = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);
    res = MemoryCompare(BUFFER_GET(bytes) + (BUFFER_LEN(bytes) - res), buffer.buffer, res);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}

ARGON_METHOD5(bytes_, find,
              "Searches bytes for a specified value and returns the position of where it was found."
              ""
              "- Parameter sub: the value to search for."
              "- Returns: index of the first position, -1 otherwise."
              ""
              "# SEE"
              "- rfind: Same as find, but returns the index of the last position.", 1, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    ArSSize pos;

    bytes = (Bytes *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    pos = support::Find(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, false);

    return IntegerNew(pos);
}

ARGON_METHOD5(bytes_, freeze,
              "Freeze bytes object."
              ""
              "If bytes is already frozen, the same object will be returned, otherwise a new frozen bytes(view) will be returned."
              "- Returns: frozen bytes object.", 0, false) {
    auto *bytes = (Bytes *) self;

    return BytesFreeze(bytes);
}

ARGON_METHOD5(bytes_, hex, "Convert bytes to str of hexadecimal numbers."
                           ""
                           "- Returns: new str object.", 0, false) {
    StringBuilder builder{};
    Bytes *bytes;

    bytes = (Bytes *) self;

    if (StringBuilderWriteHex(&builder, BUFFER_GET(bytes), BUFFER_LEN(bytes)) < 0) {
        StringBuilderClean(&builder);
        return nullptr;
    }

    return StringBuilderFinish(&builder);
}

ARGON_METHOD5(bytes_, isalnum,
              "Check if all characters in the bytes are alphanumeric (either alphabets or numbers)."
              ""
              "- Returns: true if all characters are alphanumeric, false otherwise."
              ""
              "# SEE"
              "- isalpha: Check if all characters in the bytes are alphabets."
              "- isascii: Check if all characters in the bytes are ascii."
              "- isdigit: Check if all characters in the bytes are digits.", 0, false) {
    auto *bytes = (Bytes *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if ((chr < 'A' || chr > 'Z') && (chr < 'a' || chr > 'z') && (chr < '0' || chr > '9'))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD5(bytes_, isalpha,
              "Check if all characters in the bytes are alphabets."
              ""
              "- Returns: true if all characters are alphabets, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytes are alphanumeric (either alphabets or numbers)."
              "- isascii: Check if all characters in the bytes are ascii."
              "- isdigit: Check if all characters in the bytes are digits.", 0, false) {
    auto *bytes = (Bytes *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if ((chr < 'A' || chr > 'Z') && (chr < 'a' || chr > 'z'))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}


ARGON_METHOD5(bytes_, isascii,
              "Check if all characters in the bytes are ascii."
              ""
              "- Returns: true if all characters are ascii, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytes are alphanumeric (either alphabets or numbers)."
              "- isalpha: Check if all characters in the bytes are alphabets."
              "- isdigit: Check if all characters in the bytes are digits.", 0, false) {
    auto *bytes = (Bytes *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if (chr > 0x7F)
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}


ARGON_METHOD5(bytes_, isdigit,
              "Check if all characters in the bytes are digits."
              ""
              "- Returns: true if all characters are digits, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytes are alphanumeric (either alphabets or numbers)."
              "- isalpha: Check if all characters in the bytes are alphabets."
              "- isascii: Check if all characters in the bytes are ascii.", 0, false) {
    auto *bytes = (Bytes *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if (chr < '0' || chr > '9')
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD5(bytes_, isfrozen,
              "Check if this bytes object is frozen."
              ""
              "- Returns: true if it is frozen, false otherwise.", 0, false) {
    return BoolToArBool(((Bytes *) self)->frozen);
}

ARGON_METHOD5(bytes_, join,
              "Joins the elements of an iterable to the end of the bytes."
              ""
              "- Parameter iterable: any iterable object where all the returned values are bytes-like object."
              "- Returns: new bytes where all items in an iterable are joined into one bytes.", 1, false) {
    ArBuffer buffer{};
    ArObject *item = nullptr;
    ArObject *iter;

    Bytes *bytes;
    Bytes *ret;

    ArSize idx = 0;
    ArSize len;

    bytes = (Bytes *) self;

    if ((iter = IteratorGet(argv[0])) == nullptr)
        return nullptr;

    if ((ret = BytesNew()) == nullptr)
        goto error;

    while ((item = IteratorNext(iter)) != nullptr) {
        if (!BufferGet(item, &buffer, ArBufferFlags::READ))
            goto error;

        len = buffer.len;

        if (idx > 0)
            len += bytes->view.len;

        if (!BufferViewEnlarge(&ret->view, len)) {
            BufferRelease(&buffer);
            goto error;
        }

        if (idx > 0) {
            MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(ret), BUFFER_GET(bytes), BUFFER_LEN(bytes));
            ret->view.len += BUFFER_LEN(bytes);
        }

        MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(ret), buffer.buffer, buffer.len);
        ret->view.len += buffer.len;

        BufferRelease(&buffer);
        Release(item);
        idx++;
    }

    Release(iter);
    return ret;

    error:
    Release(item);
    Release(iter);
    Release(ret);
    return nullptr;
}

ARGON_METHOD5(bytes_, rfind,
              "Searches bytes for a specified value and returns the position of where it was found."
              ""
              "- Parameter sub: the value to search for."
              "- Returns: index of the first position, -1 otherwise."
              ""
              "# SEE"
              "- find: Same as find, but returns the index of the last position.", 1, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    ArSSize pos;

    bytes = (Bytes *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    pos = support::Find(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, true);

    return IntegerNew(pos);
}

ARGON_METHOD5(bytes_, rmpostfix,
              "Returns new bytes without postfix(if present), otherwise return this object."
              ""
              "- Parameter postfix: postfix to looking for."
              "- Returns: new bytes without indicated postfix."
              ""
              "# SEE"
              "- rmprefix: Returns new bytes without prefix(if present), otherwise return this object.",
              1, false) {
    ArBuffer buffer{};
    auto *bytes = (Bytes *) self;
    int len;
    int compare;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    len = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);

    compare = MemoryCompare(BUFFER_GET(bytes) + (BUFFER_LEN(bytes) - len), buffer.buffer, len);
    BufferRelease(&buffer);

    if (compare == 0)
        return BytesNew(bytes, 0, BUFFER_LEN(bytes) - len);

    return IncRef(bytes);
}


ARGON_METHOD5(bytes_, rmprefix,
              "Returns new bytes without prefix(if present), otherwise return this object."
              ""
              "- Parameter prefix: prefix to looking for."
              "- Returns: new bytes without indicated prefix."
              ""
              "# SEE"
              "- rmpostfix: Returns new bytes without postfix(if present), otherwise return this object.", 1,
              false) {
    ArBuffer buffer{};
    auto *bytes = (Bytes *) self;
    int len;
    int compare;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    len = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);

    compare = MemoryCompare(BUFFER_GET(bytes), buffer.buffer, len);
    BufferRelease(&buffer);

    if (compare == 0)
        return BytesNew(bytes, len, BUFFER_LEN(bytes) - len);

    return IncRef(bytes);
}


ARGON_METHOD5(bytes_, split,
              "Splits bytes at the specified separator, and returns a list."
              ""
              "- Parameters:"
              " - separator: specifies the separator to use when splitting bytes."
              " - maxsplit: specifies how many splits to do."
              "- Returns: new list of bytes.", 2, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    ArObject *ret;

    bytes = (Bytes *) self;

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "bytes::split() expected integer not '%s'", AR_TYPE_NAME(argv[1]));

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    ret = BytesSplit(bytes, buffer.buffer, buffer.len, ((Integer *) argv[1])->integer);

    BufferRelease(&buffer);

    return ret;
}


ARGON_METHOD5(bytes_, startswith,
              "Returns true if bytes starts with the specified value."
              ""
              "- Parameter prefix: the value to check if the bytes starts with."
              "- Returns: true if bytes starts with the specified value, false otherwise."
              ""
              "# SEE"
              "- endswith: Returns true if bytes ends with the specified value.", 1, false) {
    ArBuffer buffer{};
    Bytes *bytes;
    int res;

    bytes = (Bytes *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    res = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);
    res = MemoryCompare(BUFFER_GET(bytes), buffer.buffer, res);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}


ARGON_METHOD5(bytes_, str, "Convert bytes to str object."
                           ""
                           "- Returns: new str object.", 0, false) {
    auto *bytes = (Bytes *) self;

    return StringNew((const char *) BUFFER_GET(bytes), BUFFER_LEN(bytes));
}


const NativeFunc bytes_methods[] = {
        bytes_count_,
        bytes_endswith_,
        bytes_find_,
        bytes_freeze_,
        bytes_hex_,
        bytes_isalnum_,
        bytes_isalpha_,
        bytes_isascii_,
        bytes_isdigit_,
        bytes_isfrozen_,
        bytes_join_,
        bytes_new_,
        bytes_rfind_,
        bytes_rmpostfix_,
        bytes_rmprefix_,
        bytes_split_,
        bytes_startswith_,
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

ArObject *bytes_add(Bytes *self, ArObject *other) {
    ArBuffer buffer = {};
    Bytes *ret;

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((ret = BytesNew(BUFFER_LEN(self) + buffer.len, true, false, self->frozen)) == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(BUFFER_GET(ret), BUFFER_GET(self), BUFFER_LEN(self));
    MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(self), buffer.buffer, buffer.len);

    BufferRelease(&buffer);

    return ret;
}

ArObject *bytes_mul(ArObject *left, ArObject *right) {
    auto *bytes = (Bytes *) left;
    auto *num = (Integer *) right;
    Bytes *ret = nullptr;

    ArSize len;

    if (!AR_TYPEOF(bytes, type_bytes_)) {
        bytes = (Bytes *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        len = BUFFER_LEN(bytes) * num->integer;

        if ((ret = BytesNew(len, true, false, bytes->frozen)) != nullptr) {
            for (ArSize i = 0; i < num->integer; i++)
                MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(bytes) * i, BUFFER_GET(bytes), BUFFER_LEN(bytes));
        }
    }

    return ret;
}

Bytes *ShiftBytes(Bytes *bytes, ArSSize pos) {
    auto ret = BytesNew(BUFFER_LEN(bytes), true, false, bytes->frozen);

    if (ret != nullptr) {
        for (ArSize i = 0; i < BUFFER_LEN(bytes); i++)
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

ArObject *bytes_iadd(Bytes *self, ArObject *other) {
    ArBuffer buffer = {};
    Bytes *ret = self;

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if (self->frozen) {
        if ((ret = BytesNew(BUFFER_LEN(self) + buffer.len, true, false, true)) == nullptr) {
            BufferRelease(&buffer);
            return nullptr;
        }

        MemoryCopy(BUFFER_GET(ret), BUFFER_GET(self), BUFFER_LEN(self));
        MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(self), buffer.buffer, buffer.len);

        BufferRelease(&buffer);
        return ret;
    }

    if (!BufferViewEnlarge(&self->view, buffer.len)) {
        BufferRelease(&buffer);
        return nullptr;
    }

    MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(ret), buffer.buffer, buffer.len);
    ret->view.len += buffer.len;

    BufferRelease(&buffer);
    return IncRef(self);
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
        (BinaryOp) bytes_iadd,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *bytes_str(Bytes *self) {
    StringBuilder sb = {};

    // Calculate length of string
    if (!StringBuilderResizeAscii(&sb, BUFFER_GET(self), BUFFER_LEN(self), 3)) // +3 b""
        return nullptr;

    // Build string
    StringBuilderWrite(&sb, (const unsigned char *) "b\"", 2);;
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

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArSize bytes_hash(Bytes *self) {
    if (!self->frozen) {
        ErrorFormat(type_unhashable_error_, "unable to hash unfrozen bytes object");
        return 0;
    }

    if (self->hash == 0)
        self->hash = HashBytes(BUFFER_GET(self), BUFFER_LEN(self));

    return self->hash;
}

bool bytes_is_true(Bytes *self) {
    return BUFFER_LEN(self) > 0;
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

ArObject *argon::object::BytesSplit(Bytes *bytes, unsigned char *pattern, ArSize plen, ArSSize maxsplit) {
    Bytes *tmp;
    List *ret;

    ArSize idx = 0;
    ArSSize end;
    ArSSize counter = 0;

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    if (maxsplit != 0) {
        while ((end = support::Find(BUFFER_GET(bytes) + idx, BUFFER_LEN(bytes) - idx, pattern, plen)) >= 0) {
            if ((tmp = BytesNew(bytes, idx, end - idx)) == nullptr)
                goto error;

            idx += end + plen;

            if (!ListAppend(ret, tmp))
                goto error;

            Release(tmp);

            if (maxsplit > -1 && ++counter >= maxsplit)
                break;
        }
    }

    if (BUFFER_LEN(bytes) - idx > 0) {
        if ((tmp = BytesNew(bytes, idx, BUFFER_LEN(bytes) - idx)) == nullptr)
            goto error;

        if (!ListAppend(ret, tmp))
            goto error;

        Release(tmp);
    }

    return ret;

    error:
    Release(tmp);
    Release(ret);
    return nullptr;
}

Bytes *argon::object::BytesNew(ArObject *object) {
    ArBuffer buffer = {};
    Bytes *bs;

    if (!IsBufferable(object))
        return nullptr;

    if (!BufferGet(object, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((bs = BytesNew(buffer.len, true, false, false)) != nullptr)
        MemoryCopy(BUFFER_GET(bs), buffer.buffer, buffer.len);

    BufferRelease(&buffer);

    return bs;
}

Bytes *argon::object::BytesNew(Bytes *stream, ArSize start, ArSize len) {
    auto bs = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (bs != nullptr) {
        BufferViewInit(&bs->view, &stream->view, start, len);
        bs->hash = 0;
        bs->frozen = stream->frozen;
    }

    return bs;
}

Bytes *argon::object::BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen) {
    auto bs = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (bs != nullptr) {
        if (!BufferViewInit(&bs->view, cap)) {
            Release(bs);
            return nullptr;
        }

        if (same_len)
            bs->view.len = cap;

        if (fill_zero)
            MemoryZero(BUFFER_GET(bs), cap);

        bs->hash = 0;
        bs->frozen = frozen;
    }

    return bs;
}

Bytes *argon::object::BytesNew(unsigned char *buffer, ArSize len, bool frozen) {
    auto *bytes = BytesNew(len, true, false, frozen);

    if (bytes != nullptr)
        MemoryCopy(BUFFER_GET(bytes), buffer, len);

    return bytes;
}

Bytes *argon::object::BytesNewHoldBuffer(unsigned char *buffer, ArSize len, ArSize cap, bool frozen) {
    auto bs = ArObjectNew<Bytes>(RCType::INLINE, &type_bytes_);

    if (bs != nullptr) {
        if (!BufferViewHoldBuffer(&bs->view, buffer, len, cap)) {
            Release(bs);
            return nullptr;
        }

        bs->hash = 0;
        bs->frozen = frozen;
    }

    return bs;
}

Bytes *argon::object::BytesFreeze(Bytes *stream) {
    Bytes *ret;

    if (stream->frozen)
        return IncRef(stream);

    if ((ret = BytesNew(stream, 0, BUFFER_LEN(stream))) == nullptr)
        return nullptr;

    ret->frozen = true;
    Hash(ret);

    return ret;
}

#undef BUFFER_GET
#undef BUFFER_LEN
#undef BUFFER_MAXLEN