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
#include "vm/datatype/support/common.h"

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

ARGON_METHOD(bytes_capitalize, capitalize,
             "Returns a capitalized version of the bytes string. \n"
             "\n"
             "- Returns: New capitalized bytes string.\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;
    Bytes *ret;

    SHARED_LOCK(self);

    if (BUFFER_LEN(self) == 0 || toupper(*BUFFER_GET(self)) == *BUFFER_GET(self)) {
        SHARED_UNLOCK(self);

        return (ArObject *) IncRef(self);
    }

    if ((ret = BytesNew(BUFFER_GET(self), BUFFER_LEN(self), self->frozen)) == nullptr)
        return nullptr;

    SHARED_UNLOCK(self);

    BUFFER_GET(ret)[0] = (unsigned char) toupper(*BUFFER_GET(ret));

    return (ArObject *) ret;
}

ARGON_METHOD(bytes_count, count,
             "Returns the number of times a specified value occurs in bytes string.\n"
             "\n"
             "- Parameter pattern: The Bytes/String to value to search for.\n"
             "- Returns: Number of times a specified value appears in the bytes string.\n",
             ": pattern", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSSize count;

    if (_self == args[0])
        return (ArObject *) IntNew(1);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    count = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    count = stratum::util::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - count), buffer.buffer, count);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return (ArObject *) IntNew(count);
}

ARGON_METHOD(bytes_clone, clone,
             "Make a clone of the bytes object.\n"
             "\n"
             "- Returns: A new bytes object identical to the current one.\n",
             nullptr, false, false) {
    return (ArObject *) BytesNew(_self);
}

ARGON_METHOD(bytes_endswith, endswith,
             "Returns true if the bytes string ends with the specified value.\n"
             "\n"
             "- Parameter pattern: The value to check if the bytes string ends with.\n"
             "- Returns: True if the bytes string ends with the specified value, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- startswith\n",
             ": pattern", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSSize res;

    if (_self == args[0])
        return BoolToArBool(true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    res = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    res = stratum::util::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - res), buffer.buffer, res);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}

ARGON_METHOD(bytes_find, find,
             "Searches the string for a specified value and returns the position of where it was found.\n"
             "\n"
             "- Parameter pattern: The value to search for.\n"
             "- Returns: Index of the first position, -1 otherwise.\n"
             "\n"
             "# SEE\n"
             "- rfind\n",
             ": pattern", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSSize index;

    if (_self == args[0])
        return BoolToArBool(true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return BoolToArBool(index);
}

ARGON_METHOD(bytes_freeze, freeze,
             "Freeze bytes object.\n"
             "\n"
             "If bytes is already frozen, the same object will be returned, "
             "otherwise a new frozen bytes(view) will be returned.\n"
             "\n"
             "- Returns: Frozen bytes object.\n",
             nullptr, false, false) {
    return (ArObject *) BytesFreeze((Bytes *) _self);
}

ARGON_METHOD(bytes_isalnum, isalnum,
             "Check if all characters in the bytes are alphanumeric (either alphabets or numbers).\n"
             "\n"
             "- Returns: True if all characters are alphanumeric, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- isalpha\n"
             "- isascii\n"
             "- isdigit\n"
             "- isxdigit\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;

    SHARED_LOCK(self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isalnum(BUFFER_GET(self)[i])) {
            SHARED_UNLOCK(self);

            return BoolToArBool(false);
        }
    }

    SHARED_UNLOCK(self);

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isalpha, isalpha,
             "Check if all characters in the bytes are alphabets.\n"
             "\n"
             "- Returns: True if all characters are alphabets, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- isalnum\n"
             "- isascii\n"
             "- isdigit\n"
             "- isxdigit\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;

    SHARED_LOCK(self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isalpha(BUFFER_GET(self)[i])) {
            SHARED_UNLOCK(self);

            return BoolToArBool(false);
        }
    }

    SHARED_UNLOCK(self);

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isascii, isascii,
             "Check if all characters in the bytes are ascii.\n"
             "\n"
             "- Returns: True if all characters are ascii, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- isalnum\n"
             "- isalpha\n"
             "- isdigit\n"
             "- isxdigit\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;

    SHARED_LOCK(self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (BUFFER_GET(self)[i] > 0x7F) {
            SHARED_UNLOCK(self);

            return BoolToArBool(false);
        }
    }

    SHARED_UNLOCK(self);

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isdigit, isdigit,
             "Check if all characters in the bytes are digits.\n"
             "\n"
             "- Returns: True if all characters are digits, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- isalnum\n"
             "- isalpha\n"
             "- isascii\n"
             "- isxdigit\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;

    SHARED_LOCK(self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isdigit(BUFFER_GET(self)[i])) {
            SHARED_UNLOCK(self);

            return BoolToArBool(false);
        }
    }

    SHARED_UNLOCK(self);

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isxdigit, isxdigit,
             "Check if all characters in the bytes are hex digits.\n"
             "\n"
             "- Returns: True if all characters are hex digits, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- isalnum\n"
             "- isalpha\n"
             "- isdigit\n"
             "- isascii\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;

    SHARED_LOCK(self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isdigit(BUFFER_GET(self)[i])) {
            SHARED_UNLOCK(self);

            return BoolToArBool(false);
        }
    }

    SHARED_UNLOCK(self);

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isfrozen, isfrozen,
             "Check if this bytes object is frozen.\n"
             "\n"
             "- Returns: True if it is frozen, false otherwise.\n",
             nullptr, false, false) {
    return BoolToArBool(((Bytes *) _self)->frozen);
}

ARGON_METHOD(bytes_lower, lower,
             "Return a copy of the bytes string converted to lowercase.\n"
             "\n"
             "- Returns: New bytes string with all characters converted to lowercase.\n",
             nullptr, false, false) {
    Bytes *ret;

    if ((ret = BytesNew(_self)) != nullptr) {
        for (ArSize i = 0; i < BUFFER_LEN(ret); i++)
            BUFFER_GET(ret)[i] = (unsigned char) tolower(BUFFER_GET(ret)[i]);

        ret->frozen = ((Bytes *) _self)->frozen;
    }

    return (ArObject *) ret;
}


ARGON_METHOD(bytes_rfind, rfind,
             "Searches the string for a specified value and returns the last position of where it was found.\n"
             "\n"
             "- Parameter pattern: The value to search for.\n"
             "- Returns: Index of the last position, -1 otherwise.\n"
             "\n"
             "# SEE\n"
             "- find\n",
             ": pattern", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSSize index;

    if (_self == args[0])
        return BoolToArBool(true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length, true);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return BoolToArBool(index);
}

ARGON_METHOD(bytes_rmpostfix, rmpostfix,
             "Returns new bytes without postfix(if present), otherwise return this object.\n"
             "\n"
             "- Parameter postfix: Postfix to looking for.\n"
             "- Returns: New bytes without indicated postfix.\n"
             "\n"
             "# SEE\n"
             "- rmprefix\n",
             ": postfix", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSize len;
    int compare;

    if (_self == args[0])
        return (ArObject *) BytesNew(nullptr, 0, true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    len = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);

    compare = argon::vm::memory::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - len), buffer.buffer, len);

    BufferRelease(&buffer);

    if (compare == 0) {
        auto *ret = BytesNew(BUFFER_GET(self), BUFFER_LEN(self) - len, self->frozen);

        SHARED_UNLOCK(self);

        return (ArObject *) ret;
    }

    SHARED_UNLOCK(self);

    return (ArObject *) IncRef(self);
}

ARGON_METHOD(bytes_rmprefix, rmprefix,
             "Returns new bytes without prefix(if present), otherwise return this object.\n"
             "\n"
             "- Parameter prefix: Prefix to looking for.\n"
             "- Returns: New bytes without indicated prefix.\n"
             "\n"
             "# SEE\n"
             "- rmpostfix\n",
             ": prefix", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSize len;
    int compare;

    if (_self == args[0])
        return (ArObject *) BytesNew(nullptr, 0, true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    len = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);

    compare = argon::vm::memory::MemoryCompare(BUFFER_GET(self), buffer.buffer, len);

    BufferRelease(&buffer);

    if (compare == 0) {
        auto *ret = BytesNew(BUFFER_GET(self) + len, BUFFER_LEN(self) - len, self->frozen);

        SHARED_UNLOCK(self);

        return (ArObject *) ret;
    }

    SHARED_UNLOCK(self);

    return (ArObject *) IncRef(self);
}

ARGON_METHOD(bytes_split, split,
             "Splits the bytes string at the specified separator, and returns a list.\n"
             "\n"
             "- Parameters:\n"
             " - pattern: Specifies the separator to use when splitting the bytes string.\n"
             " - maxsplit: Specifies how many splits to do.\n"
             "- Returns: New list of bytes string.\n",
             ": pattern, i: maxsplit", false, false) {
    ArBuffer buffer{};
    const unsigned char *pattern = nullptr;
    ArObject *ret;

    ArSize plen = 0;

    if (_self == args[0]) {
        ErrorFormat(kValueError[0], "cannot use the object to be split as a pattern");
        return nullptr;
    }

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    if (!IsNull(args[0])) {
        pattern = buffer.buffer;
        plen = buffer.length;

        if (plen == 0) {
            ErrorFormat(kValueError[0], "empty separator");
            return nullptr;
        }
    }

    SHARED_LOCK((Bytes *) _self);

    ret = support::Split(BUFFER_GET((Bytes *) _self), pattern, (support::SplitChunkNewFn<Bytes>) BytesNew,
                         BUFFER_LEN((Bytes *) _self), plen, ((Integer *) args[1])->sint);

    SHARED_UNLOCK((Bytes *) _self);

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD(bytes_startswith, startswith,
             "Returns true if the bytes string starts with the specified value.\n"
             "\n"
             "- Parameter pattern: The value to check if the string starts with.\n"
             "- Returns: True if the string starts with the specified value, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- endswith\n",
             ": pattern", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArSSize res;

    if (_self == args[0])
        return BoolToArBool(true);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    res = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    res = stratum::util::MemoryCompare(BUFFER_GET(self), buffer.buffer, res);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return BoolToArBool(res == 0);
}

ARGON_METHOD(bytes_tohex, tohex,
             "Convert bytes to string of hexadecimal numbers.\n"
             "\n"
             "- Returns: New string object.\n",
             nullptr, false, false) {
    StringBuilder builder{};
    auto *self = (Bytes *) _self;
    ArObject *ret = nullptr;

    SHARED_LOCK(self);

    builder.WriteHex(BUFFER_GET(self), BUFFER_LEN(self));

    SHARED_UNLOCK(self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ARGON_METHOD(bytes_tostr, tostr,
             "Convert bytes to str object.\n"
             "\n"
             "- Returns: New str object.\n",
             nullptr, false, false) {
    StringBuilder builder{};
    auto *self = (Bytes *) _self;
    ArObject *ret;

    SHARED_LOCK(self);

    builder.Write(BUFFER_GET(self), BUFFER_LEN(self), 0);

    SHARED_UNLOCK(self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ARGON_METHOD(bytes_upper, upper,
             "Return a copy of the bytes string converted to uppercase.\n"
             "\n"
             "- Returns: New bytes string with all characters converted to uppercase.\n",
             nullptr, false, false) {
    Bytes *ret;

    if ((ret = BytesNew(_self)) != nullptr) {
        for (ArSize i = 0; i < BUFFER_LEN(ret); i++)
            BUFFER_GET(ret)[i] = (unsigned char) toupper(BUFFER_GET(ret)[i]);

        ret->frozen = ((Bytes *) _self)->frozen;
    }

    return (ArObject *) ret;
}

const FunctionDef bytes_method[] = {
        bytes_capitalize,
        bytes_count,
        bytes_clone,
        bytes_endswith,
        bytes_find,
        bytes_freeze,
        bytes_isalnum,
        bytes_isalpha,
        bytes_isascii,
        bytes_isdigit,
        bytes_isxdigit,
        bytes_isfrozen,
        bytes_lower,
        bytes_tohex,
        bytes_tostr,
        bytes_rfind,
        bytes_rmpostfix,
        bytes_rmprefix,
        bytes_split,
        bytes_startswith,
        bytes_upper,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots bytes_objslot = {
        bytes_method,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

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
    ArObject *ret = nullptr;

    SHARED_LOCK(self);

    builder.Write((const unsigned char *) "b\"", 2, BUFFER_LEN(self) + 1);
    builder.WriteEscaped(BUFFER_GET(self), BUFFER_LEN(self), 1);
    builder.Write((const unsigned char *) "\"", 1, 0);

    SHARED_UNLOCK(self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
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
        &bytes_objslot,
        &bytes_subscript,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_bytes_ = &BytesType;

Bytes *argon::vm::datatype::BytesFreeze(Bytes *bytes) {
    Bytes *ret;

    if (bytes->frozen)
        return IncRef(bytes);

    if ((ret = BytesNew(bytes, 0, BUFFER_LEN(bytes))) == nullptr)
        return nullptr;

    ret->frozen = true;

    Hash((ArObject *) ret, &ret->hash);

    return ret;
}

Bytes *argon::vm::datatype::BytesNew(ArObject *object) {
    ArBuffer buffer = {};
    Bytes *bs;

    if (!BufferGet(object, &buffer, BufferFlags::READ))
        return nullptr;

    if ((bs = BytesNew(buffer.length, true, false, false)) != nullptr)
        memory::MemoryCopy(BUFFER_GET(bs), buffer.buffer, buffer.length);

    BufferRelease(&buffer);

    return bs;
}

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