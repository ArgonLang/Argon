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
#include "hash_magic.h"

#define BUFFER_GET(bs)              (bs->view.buffer)
#define BUFFER_LEN(bs)              (bs->view.len)
#define BUFFER_MAXLEN(left, right)  (BUFFER_LEN(left) > BUFFER_LEN(right) ? BUFFER_LEN(right) : BUFFER_LEN(self))

using namespace argon::memory;
using namespace argon::object;

bool bytestream_get_buffer(ByteStream *self, ArBuffer *buffer, ArBufferFlags flags) {
    return BufferSimpleFill(self, buffer, flags, BUFFER_GET(self), BUFFER_LEN(self), self->frozen);
}

const BufferSlots bytestream_buffer = {
        (BufferGetFn) bytestream_get_buffer,
        nullptr
};

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

    if (self->frozen) {
        ErrorFormat(type_type_error_, "unable to set item to frozen bytes object");
        return false;
    }

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
        if ((ret = ByteStreamNew(slice_len, true, false, self->frozen)) == nullptr)
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

ARGON_FUNCTION5(bytestream_, new, "Creates bytestream object."
                                  ""
                                  "The src parameter is optional, in case of call without src parameter an empty zero-length"
                                  "bytestream object will be constructed."
                                  ""
                                  "- Parameter [src]: integer or bytes-like object."
                                  "- Returns: construct a new bytestream object.", 0, true) {
    IntegerUnderlying size = 0;

    if (!VariadicCheckPositional("bytestream::new", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (!AR_TYPEOF(*argv, type_integer_))
            return ByteStreamNew(*argv);

        size = ((Integer *) *argv)->integer;
    }

    return ByteStreamNew(size, true, true, false);
}

ARGON_METHOD5(bytestream_, count,
              "Returns the number of times a specified value occurs in bytestream."
              ""
              "- Parameter sub: subsequence to search."
              "- Returns: number of times a specified value appears in bytestream.", 1, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    ArSSize n;

    bytes = (ByteStream *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    n = support::Count(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, -1);

    BufferRelease(&buffer);

    return IntegerNew(n);
}

ARGON_METHOD5(bytestream_, clone,
              "Returns the number of times a specified value occurs in bytestream."
              ""
              "- Parameter sub: subsequence to search."
              "- Returns: number of times a specified value appears in the string.", 0, false) {
    return ByteStreamNew(self);
}

ARGON_METHOD5(bytestream_, endswith,
              "Returns true if bytestream ends with the specified value."
              ""
              "- Parameter suffix: the value to check if the bytestream ends with."
              "- Returns: true if bytestream ends with the specified value, false otherwise."
              ""
              "# SEE"
              "- startswith: Returns true if bytestream starts with the specified value.", 1, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    int res;

    bytes = (ByteStream *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    res = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);
    res = MemoryCompare(BUFFER_GET(bytes) + (BUFFER_LEN(bytes) - res), buffer.buffer, res);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}

ARGON_METHOD5(bytestream_, find,
              "Searches bytestream for a specified value and returns the position of where it was found."
              ""
              "- Parameter sub: the value to search for."
              "- Returns: index of the first position, -1 otherwise."
              ""
              "# SEE"
              "- rfind: Same as find, but returns the index of the last position.", 1, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    ArSSize pos;

    bytes = (ByteStream *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    pos = support::Find(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, false);

    return IntegerNew(pos);
}

ARGON_METHOD5(bytestream_, freeze,
              "Freeze bytes object."
              ""
              "If bytes is already frozen, the same object will be returned, otherwise a new frozen bytes(view) will be returned."
              "- Returns: frozen bytes object.", 0, false) {
    auto *bytes = (ByteStream *) self;

    return ByteStreamFreeze(bytes);
}

ARGON_METHOD5(bytestream_, hex, "Convert bytestream to str of hexadecimal numbers."
                                ""
                                "- Returns: new str object.", 0, false) {
    StringBuilder builder{};
    ByteStream *bytes;

    bytes = (ByteStream *) self;

    if (StringBuilderWriteHex(&builder, BUFFER_GET(bytes), BUFFER_LEN(bytes)) < 0) {
        StringBuilderClean(&builder);
        return nullptr;
    }

    return StringBuilderFinish(&builder);
}

ARGON_METHOD5(bytestream_, isalnum,
              "Check if all characters in the bytestream are alphanumeric (either alphabets or numbers)."
              ""
              "- Returns: true if all characters are alphanumeric, false otherwise."
              ""
              "# SEE"
              "- isalpha: Check if all characters in the bytestream are alphabets."
              "- isascii: Check if all characters in the bytestream are ascii."
              "- isdigit: Check if all characters in the bytestream are digits.", 0, false) {
    auto *bytes = (ByteStream *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if ((chr < 'A' || chr > 'Z') && (chr < 'a' || chr > 'z') && (chr < '0' || chr > '9'))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD5(bytestream_, isalpha,
              "Check if all characters in the bytestream are alphabets."
              ""
              "- Returns: true if all characters are alphabets, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytestream are alphanumeric (either alphabets or numbers)."
              "- isascii: Check if all characters in the bytestream are ascii."
              "- isdigit: Check if all characters in the bytestream are digits.", 0, false) {
    auto *bytes = (ByteStream *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if ((chr < 'A' || chr > 'Z') && (chr < 'a' || chr > 'z'))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}


ARGON_METHOD5(bytestream_, isascii,
              "Check if all characters in the bytestream are ascii."
              ""
              "- Returns: true if all characters are ascii, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytestream are alphanumeric (either alphabets or numbers)."
              "- isalpha: Check if all characters in the bytestream are alphabets."
              "- isdigit: Check if all characters in the bytestream are digits.", 0, false) {
    auto *bytes = (ByteStream *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if (chr > 0x7F)
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}


ARGON_METHOD5(bytestream_, isdigit,
              "Check if all characters in the bytestream are digits."
              ""
              "- Returns: true if all characters are digits, false otherwise."
              ""
              "# SEE"
              "- isalnum: Check if all characters in the bytestream are alphanumeric (either alphabets or numbers)."
              "- isalpha: Check if all characters in the bytestream are alphabets."
              "- isascii: Check if all characters in the bytestream are ascii.", 0, false) {
    auto *bytes = (ByteStream *) self;
    int chr;

    for (ArSize i = 0; i < BUFFER_LEN(bytes); i++) {
        chr = BUFFER_GET(bytes)[i];

        if (chr < '0' || chr > '9')
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD5(bytestream_, isfrozen,
              "Check if this bytes object is frozen."
              ""
              "- Returns: true if it is frozen, false otherwise.", 0, false) {
    return BoolToArBool(((ByteStream *) self)->frozen);
}

ARGON_METHOD5(bytestream_, join,
              "Joins the elements of an iterable to the end of the bytestream."
              ""
              "- Parameter iterable: any iterable object where all the returned values are bytes-like object."
              "- Returns: new bytestream where all items in an iterable are joined into one bytestream.", 1, false) {
    ArBuffer buffer{};
    ArObject *item = nullptr;
    ArObject *iter;

    ByteStream *bytes;
    ByteStream *ret;

    ArSize idx = 0;
    ArSize len;

    bytes = (ByteStream *) self;

    if ((iter = IteratorGet(argv[0])) == nullptr)
        return nullptr;

    if ((ret = ByteStreamNew()) == nullptr)
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

ARGON_METHOD5(bytestream_, rfind,
              "Searches bytestream for a specified value and returns the position of where it was found."
              ""
              "- Parameter sub: the value to search for."
              "- Returns: index of the first position, -1 otherwise."
              ""
              "# SEE"
              "- find: Same as find, but returns the index of the last position.", 1, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    ArSSize pos;

    bytes = (ByteStream *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    pos = support::Find(BUFFER_GET(bytes), BUFFER_LEN(bytes), buffer.buffer, buffer.len, true);

    return IntegerNew(pos);
}

ARGON_METHOD5(bytestream_, rmpostfix,
              "Returns new bytestream without postfix(if present), otherwise return this object."
              ""
              "- Parameter postfix: postfix to looking for."
              "- Returns: new bytestream without indicated postfix."
              ""
              "# SEE"
              "- rmprefix: Returns new bytestream without prefix(if present), otherwise return this object.",
              1, false) {
    ArBuffer buffer{};
    auto *bytes = (ByteStream *) self;
    int len;
    int compare;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    len = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);

    compare = MemoryCompare(BUFFER_GET(bytes) + (BUFFER_LEN(bytes) - len), buffer.buffer, len);
    BufferRelease(&buffer);

    if (compare == 0)
        return ByteStreamNew(bytes, 0, BUFFER_LEN(bytes) - len);

    return IncRef(bytes);
}


ARGON_METHOD5(bytestream_, rmprefix,
              "Returns new bytestream without prefix(if present), otherwise return this object."
              ""
              "- Parameter prefix: prefix to looking for."
              "- Returns: new bytestream without indicated prefix."
              ""
              "# SEE"
              "- rmpostfix: Returns new bytestream without postfix(if present), otherwise return this object.", 1,
              false) {
    ArBuffer buffer{};
    auto *bytes = (ByteStream *) self;
    int len;
    int compare;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    len = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);

    compare = MemoryCompare(BUFFER_GET(bytes), buffer.buffer, len);
    BufferRelease(&buffer);

    if (compare == 0)
        return ByteStreamNew(bytes, len, BUFFER_LEN(bytes) - len);

    return IncRef(bytes);
}


ARGON_METHOD5(bytestream_, split,
              "Splits bytestream at the specified separator, and returns a list."
              ""
              "- Parameters:"
              " - separator: specifies the separator to use when splitting bytestream."
              " - maxsplit: specifies how many splits to do."
              "- Returns: new list of bytestream.", 2, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    ArObject *ret;

    bytes = (ByteStream *) self;

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "bytestream::split() expected integer not '%s'", AR_TYPE_NAME(argv[1]));

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    ret = ByteStreamSplit(bytes, buffer.buffer, buffer.len, ((Integer *) argv[1])->integer);

    BufferRelease(&buffer);

    return ret;
}


ARGON_METHOD5(bytestream_, startswith,
              "Returns true if bytestream starts with the specified value."
              ""
              "- Parameter prefix: the value to check if the bytestream starts with."
              "- Returns: true if bytestream starts with the specified value, false otherwise."
              ""
              "# SEE"
              "- endswith: Returns true if bytestream ends with the specified value.", 1, false) {
    ArBuffer buffer{};
    ByteStream *bytes;
    int res;

    bytes = (ByteStream *) self;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    res = BUFFER_LEN(bytes) > buffer.len ? buffer.len : BUFFER_LEN(bytes);
    res = MemoryCompare(BUFFER_GET(bytes), buffer.buffer, res);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}


ARGON_METHOD5(bytestream_, str, "Convert bytestream to str object."
                                ""
                                "- Returns: new str object.", 0, false) {
    auto *bytes = (ByteStream *) self;

    return StringNew((const char *) BUFFER_GET(bytes), BUFFER_LEN(bytes));
}


const NativeFunc bytestream_methods[] = {
        bytestream_count_,
        bytestream_endswith_,
        bytestream_find_,
        bytestream_freeze_,
        bytestream_hex_,
        bytestream_isalnum_,
        bytestream_isalpha_,
        bytestream_isascii_,
        bytestream_isdigit_,
        bytestream_isfrozen_,
        bytestream_join_,
        bytestream_new_,
        bytestream_rfind_,
        bytestream_rmpostfix_,
        bytestream_rmprefix_,
        bytestream_split_,
        bytestream_startswith_,
        bytestream_str_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots bytestream_obj = {
        bytestream_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *bytestream_add(ByteStream *self, ArObject *other) {
    ArBuffer buffer = {};
    ByteStream *ret;

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((ret = ByteStreamNew(BUFFER_LEN(self) + buffer.len, true, false, self->frozen)) == nullptr) {
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

        if ((ret = ByteStreamNew(len, true, false, bytes->frozen)) != nullptr) {
            for (size_t i = 0; i < num->integer; i++)
                MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(bytes) * i, BUFFER_GET(bytes), BUFFER_LEN(bytes));
        }
    }

    return ret;
}

ByteStream *ShiftBytestream(ByteStream *bytes, ArSSize pos) {
    auto ret = ByteStreamNew(BUFFER_LEN(bytes), true, false, bytes->frozen);

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
    ByteStream *ret = self;

    if (!IsBufferable(other))
        return nullptr;

    if (!BufferGet(other, &buffer, ArBufferFlags::READ))
        return nullptr;

    if (self->frozen) {
        if ((ret = ByteStreamNew(BUFFER_LEN(self) + buffer.len, true, false, true)) == nullptr) {
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

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArSize bytestream_hash(ByteStream *self) {
    if (!self->frozen) {
        ErrorFormat(type_unhashable_error_, "unable to hash unfrozen bytes object");
        return 0;
    }

    if (self->hash == 0)
        self->hash = HashBytes(BUFFER_GET(self), BUFFER_LEN(self));

    return self->hash;
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
        nullptr,
        (VoidUnaryOp) bytestream_cleanup,
        nullptr,
        (CompareOp) bytestream_compare,
        (BoolUnaryOp) bytestream_is_true,
        (SizeTUnaryOp) bytestream_hash,
        (UnaryOp) bytestream_str,
        (UnaryOp) bytestream_iter_get,
        (UnaryOp) bytestream_iter_rget,
        &bytestream_buffer,
        nullptr,
        nullptr,
        nullptr,
        &bytestream_obj,
        &bytestream_sequence,
        &bytestream_ops,
        nullptr,
        nullptr
};

ArObject *argon::object::ByteStreamSplit(ByteStream *bytes, unsigned char *pattern, ArSize plen, ArSSize maxsplit) {
    ByteStream *tmp;
    List *ret;

    ArSize idx = 0;
    ArSSize end;
    ArSSize counter = 0;

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    if (maxsplit != 0) {
        while ((end = support::Find(BUFFER_GET(bytes) + idx, BUFFER_LEN(bytes) - idx, pattern, plen)) >= 0) {
            if ((tmp = ByteStreamNew(bytes, idx, end - idx)) == nullptr)
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
        if ((tmp = ByteStreamNew(bytes, idx, BUFFER_LEN(bytes) - idx)) == nullptr)
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

ByteStream *argon::object::ByteStreamNew(ArObject *object) {
    ArBuffer buffer = {};
    ByteStream *bs;

    if (!IsBufferable(object))
        return nullptr;

    if (!BufferGet(object, &buffer, ArBufferFlags::READ))
        return nullptr;

    if ((bs = ByteStreamNew(buffer.len, true, false, false)) != nullptr)
        MemoryCopy(BUFFER_GET(bs), buffer.buffer, buffer.len);

    BufferRelease(&buffer);

    return bs;
}

ByteStream *argon::object::ByteStreamNew(ByteStream *stream, ArSize start, ArSize len) {
    auto bs = ArObjectNew<ByteStream>(RCType::INLINE, &type_bytestream_);

    if (bs != nullptr) {
        BufferViewInit(&bs->view, &stream->view, start, len);
        bs->hash = 0;
        bs->frozen = stream->frozen;
    }

    return bs;
}

ByteStream *argon::object::ByteStreamNew(ArSize cap, bool same_len, bool fill_zero, bool frozen) {
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

        bs->hash = 0;
        bs->frozen = frozen;
    }

    return bs;
}

ByteStream *argon::object::ByteStreamFreeze(ByteStream *stream) {
    ByteStream *ret;

    if (stream->frozen)
        return IncRef(stream);

    if ((ret = ByteStreamNew(stream, 0, BUFFER_LEN(stream))) == nullptr)
        return nullptr;

    ret->frozen = true;
    Hash(ret);

    return ret;
}

#undef BUFFER_GET
#undef BUFFER_LEN
#undef BUFFER_MAXLEN