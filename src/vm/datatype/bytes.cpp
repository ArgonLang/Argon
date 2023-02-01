// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include <vm/runtime.h>

#include <vm/datatype/support/common.h>

#include "boolean.h"
#include "bounds.h"
#include "error.h"
#include "hash_magic.h"
#include "integer.h"
#include "stringbuilder.h"
#include "stringformatter.h"

#include "bytes.h"

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

ARGON_FUNCTION(bytes_bytes, Bytes,
               "Creates bytes object.\n"
               "\n"
               "The src parameter is optional, in case of call without src parameter an empty zero-length"
               "bytes object will be constructed.\n"
               "\n"
               "- Parameter src: Integer or bytes-like object.\n"
               "- Returns: Construct a new bytes object.\n",
               nullptr, true, false) {
    if (!VariadicCheckPositional(bytes_bytes.name, (unsigned int) argc, 0, 1))
        return nullptr;

    if (argc == 1) {
        if (AR_TYPEOF(args[0], type_int_)) {
            if (((Integer *) *args)->sint < 0) {
                ErrorFormat(kValueError[0], "cannot create a negative length bytes string");
                return nullptr;
            }

            return (ArObject *) BytesNew(((Integer *) *args)->sint, true, true, false);
        } else if (AR_TYPEOF(args[0], type_uint_))
            return (ArObject *) BytesNew(((Integer *) *args)->uint, true, true, false);

        return (ArObject *) BytesNew(*args);
    }

    return (ArObject *) BytesNew(0, true, true, false);
}

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

ARGON_METHOD(bytes_copy, copy,
             "Copies a specified number of bytes from a bytes-like object.\n"
             "\n"
             "- Parameters:\n"
             "  - src: Source buffer, any bytes-like object.\n"
             "  - soff: Source offset.\n"
             "  - doff: Destination offset.\n"
             "  - len: Number of bytes to copy.\n"
             " - Returns: Number of bytes copied.\n",
             ": src, iu: soff, iu: doff, iu: len", false, false) {
    auto *self = (Bytes *) _self;
    auto *src = args[0];
    auto soff = ((Integer *) args[1])->uint;
    auto doff = ((Integer *) args[2])->uint;
    auto cplen = ((Integer *) args[3])->uint;

    ArBuffer buffer{};

    if (AR_TYPEOF(args[1], type_int_) && ((Integer *) args[1])->sint < 0) {
        ErrorFormat(kValueError[0], "invalid negative source offset");
        return nullptr;
    }

    if (AR_TYPEOF(args[2], type_int_) && ((Integer *) args[2])->sint < 0) {
        ErrorFormat(kValueError[0], "invalid negative destination offset");
        return nullptr;
    }

    if (AR_TYPEOF(args[3], type_int_) && ((Integer *) args[3])->sint < 0) {
        ErrorFormat(kValueError[0], "invalid negative length");
        return nullptr;
    }

    if (self->frozen) {
        ErrorFormat(kValueError[0], "frozen Bytes object cannot be used as destination for copy");
        return nullptr;
    }

    if (!BufferGet(src, &buffer, BufferFlags::READ))
        return nullptr;

    if (_self != src)
        VIEW_GET(self).WritableBufferLock();

    if (cplen > BUFFER_LEN(self) - doff)
        cplen = BUFFER_LEN(self) - doff;

    if (cplen > buffer.length - soff)
        cplen = buffer.length - soff;

    argon::vm::memory::MemoryCopy(BUFFER_GET(self) + doff, buffer.buffer + soff, cplen);

    if (_self != src)
        VIEW_GET(self).WritableRelease();

    BufferRelease(&buffer);

    return (ArObject *) IntNew((IntegerUnderlying) cplen);
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
        return (ArObject *) IntNew(0);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return (ArObject *) IntNew(index);
}

ARGON_METHOD(bytes_findbyte, findbyte,
             "Searches the bytes string for a specified value and returns the position of where it was found.\n"
             "\n"
             "- Parameters:\n"
             "  - offset: Start offset.\n"
             "  - byte: The value to search for.\n"
             "- Returns: Index of the first position, -1 otherwise.\n",
             "iu: offset, u: byte", false, false) {
    auto *self = (Bytes *) _self;
    ArSize start = ((Integer *) args[0])->uint;
    auto pattern = (unsigned char) ((Integer *) args[1])->uint;

    SHARED_LOCK(self);

    for (ArSize i = start; i < BUFFER_LEN(self); i++) {
        if (BUFFER_GET(self)[i] == pattern) {
            SHARED_UNLOCK(self);
            return (ArObject *) IntNew((IntegerUnderlying) (i - start));
        }
    }

    SHARED_UNLOCK(self);

    return (ArObject *) IntNew(-1);
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

ARGON_METHOD(bytes_join, join,
             "Joins the elements of an iterable to the end of the bytes string.\n"
             "\n"
             "- Parameter iterable: Any iterable object where all the returned values are bufferable.\n"
             "- Returns: New bytes string where all items in an iterable are joined into one bytes string.\n",
             ": iterable", false, false) {
    ArBuffer buffer{};
    auto *self = (Bytes *) _self;
    ArObject *item;
    ArObject *iter;
    Bytes *ret;

    ArSize idx = 0;
    ArSize len;

    if ((iter = IteratorGet(args[0], false)) == nullptr)
        return nullptr;

    if ((ret = BytesNew(0, true, false, false)) == nullptr) {
        Release(iter);
        return nullptr;
    }

    SHARED_LOCK(self);

    while ((item = IteratorNext(iter)) != nullptr) {
        const unsigned char *item_buf = BUFFER_GET(self);
        len = BUFFER_LEN(self);

        if (item != _self) {
            if (!BufferGet(item, &buffer, BufferFlags::READ))
                goto ERROR;

            item_buf = buffer.buffer;
            len = buffer.length;
        }

        if (idx > 0)
            len += BUFFER_LEN(self);

        if (!BufferViewEnlarge(&ret->view, len)) {
            BufferRelease(&buffer);
            goto ERROR;
        }

        if (idx > 0) {
            argon::vm::memory::MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(ret), BUFFER_GET(self), BUFFER_LEN(self));
            BUFFER_LEN(ret) += BUFFER_LEN(self);
        }

        argon::vm::memory::MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(ret), item_buf, len);
        BUFFER_LEN(ret) += len;

        if (item != _self)
            BufferRelease(&buffer);

        Release(item);

        idx++;
    }

    SHARED_UNLOCK(self);

    Release(iter);

    return (ArObject *) ret;

    ERROR:
    SHARED_UNLOCK(self);

    Release(item);
    Release(iter);
    Release(ret);
    return nullptr;
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
        return (ArObject *) IntNew(0);

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    SHARED_LOCK(self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length, true);

    SHARED_UNLOCK(self);

    BufferRelease(&buffer);

    return (ArObject *) IntNew(index);
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
             "Splits the bytes string at the specified separator and returns a list.\n"
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
        bytes_bytes,

        bytes_capitalize,
        bytes_count,
        bytes_copy,
        bytes_clone,
        bytes_endswith,
        bytes_find,
        bytes_findbyte,
        bytes_freeze,
        bytes_isalnum,
        bytes_isalpha,
        bytes_isascii,
        bytes_isdigit,
        bytes_isxdigit,
        bytes_isfrozen,
        bytes_join,
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
    /*
     * Write lock is needed to allow recursive use by the same thread!
     * See bytes_mod for more information.
     */

    bool ok;

    VIEW_GET(self).WritableBufferLock();

    ok = BufferSimpleFill((ArObject *) self, buffer, flags, BUFFER_GET(self), 1, BUFFER_LEN(self), !self->frozen);

    if (!ok)
        VIEW_GET(self).WritableRelease();

    return ok;
}

void bytes_rel_buffer(ArBuffer *buffer) {
    auto *self = (Bytes *) buffer->object;

    VIEW_GET(self).WritableRelease();
}

const BufferSlots bytes_buffer = {
        (BufferGetFn) bytes_get_buffer,
        bytes_rel_buffer
};

ArObject *bytes_add(Bytes *left, Bytes *right) {
    if (AR_TYPEOF(left, type_bytes_) && AR_SAME_TYPE(left, right))
        return (ArObject *) BytesConcat(left, right);

    return nullptr;
}

ArObject *bytes_mod(Bytes *left, ArObject *args) {
    /*
     * To avoid the following scenario:
     *
     *      var b = "hello %s"
     *      var result = b % b
     *
     * The mutex in write mode is used since it is the only mode that supports recursive blocking by the same thread!
     */
    unsigned char *buffer;
    Bytes *ret;

    ArSize out_length;
    ArSize out_cap;

    VIEW_GET(left).WritableBufferLock();

    StringFormatter fmt((const char *) BUFFER_GET(left), BUFFER_LEN(left), args, true);

    if ((buffer = fmt.Format(&out_length, &out_cap)) == nullptr) {
        auto *err = fmt.GetError();

        argon::vm::Panic((ArObject *) err);

        Release(err);

        VIEW_GET(left).WritableRelease();

        return nullptr;
    }

    VIEW_GET(left).WritableRelease();

    if ((ret = BytesNew(0, false, false, false)) != nullptr) {
        BUFFER_GET(ret) = buffer;

        ret->view.length = out_length;
        ret->view.shared->capacity = out_cap;

        fmt.ReleaseOwnership();
    }

    return (ArObject *) ret;
}

ArObject *bytes_mul(Bytes *left, const ArObject *right) {
    auto *l = left;
    Bytes *ret = nullptr;
    UIntegerUnderlying times;

    // int * bytes -> bytes * int
    if (!AR_TYPEOF(left, type_bytes_)) {
        l = (Bytes *) right;
        right = (const ArObject *) left;
    }

    if (AR_TYPEOF(right, type_uint_)) {
        SHARED_LOCK(l);

        times = ((const Integer *) right)->uint;
        if ((ret = BytesNew(BUFFER_LEN(l) * times, true, false, false)) != nullptr) {
            while (times--)
                argon::vm::memory::MemoryCopy(BUFFER_GET(ret) + (BUFFER_LEN(l) * times), BUFFER_GET(l), BUFFER_LEN(l));
        }

        SHARED_UNLOCK(l);
    }

    return (ArObject *) ret;
}


const OpSlots bytes_ops = {
        (BinaryOp) bytes_add,
        nullptr,
        (BinaryOp) bytes_mul,
        nullptr,
        nullptr,
        (BinaryOp) bytes_mod,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) bytes_add,
        nullptr,
        nullptr,
        nullptr
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

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArObject *bytes_iter(Bytes *self, bool reverse) {
    auto *bi = MakeObject<BytesIterator>(type_bytes_iterator_);

    if (bi != nullptr) {
        new(&bi->lock)std::mutex;

        bi->iterable = IncRef(self);
        bi->index = 0;
        bi->reverse = reverse;
    }

    return (ArObject *) bi;
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
        (UnaryBoolOp) bytes_iter,
        nullptr,
        &bytes_buffer,
        nullptr,
        &bytes_objslot,
        &bytes_subscript,
        &bytes_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_bytes_ = &BytesType;

Bytes *argon::vm::datatype::BytesConcat(Bytes *left, Bytes *right) {
    Bytes *ret;

    SHARED_LOCK(left);

    if (left != right)
        SHARED_LOCK(right);

    if ((ret = BytesNew(BUFFER_LEN(left) + BUFFER_LEN(right), true, false, false)) != nullptr) {
        memory::MemoryCopy(BUFFER_GET(ret), BUFFER_GET(left), BUFFER_LEN(left));
        memory::MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(left), BUFFER_GET(right), BUFFER_LEN(right));
    }

    SHARED_UNLOCK(left);

    if (left != right)
        SHARED_UNLOCK(right);

    return ret;
}

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

Bytes *argon::vm::datatype::BytesNewHoldBuffer(unsigned char *buffer, ArSize cap, ArSize len, bool frozen) {
    auto *bs = MakeObject<Bytes>(type_bytes_);

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

// BYTES ITERATOR

ArObject *bytesiterator_iter_next(BytesIterator *self) {
    unsigned char byte;

    std::unique_lock iter_lock(self->lock);

    SHARED_LOCK(self->iterable);

    if (!self->reverse) {
        if (self->index < BUFFER_LEN(self->iterable)) {
            byte = *(BUFFER_GET(self->iterable) + self->index);

            SHARED_UNLOCK(self->iterable);

            self->index++;

            return (ArObject *) UIntNew(byte);
        }

        SHARED_UNLOCK(self->iterable);

        return nullptr;
    }

    if (BUFFER_LEN(self->iterable) - self->index == 0) {
        SHARED_UNLOCK(self->iterable);

        return nullptr;
    }

    byte = *(BUFFER_GET(self->iterable) + (BUFFER_LEN(self->iterable) - self->index));

    SHARED_UNLOCK(self->iterable);

    self->index++;

    return (ArObject *) UIntNew(byte);
}

TypeInfo BytesIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "BytesIterator",
        nullptr,
        nullptr,
        sizeof(BytesIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) IteratorDtor,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        IteratorIter,
        (UnaryOp) bytesiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_bytes_iterator_ = &BytesIteratorType;