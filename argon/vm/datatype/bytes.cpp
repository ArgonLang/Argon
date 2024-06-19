// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/support/common.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/hash_magic.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/stringbuilder.h>
#include <argon/vm/datatype/stringformatter.h>

#include <argon/vm/datatype/bytes.h>

#define BUFFER_FROZEN(bs)           ((bs)->view.shared->IsFrozen())
#define BUFFER_GET(bs)              ((bs)->view.buffer)
#define BUFFER_LEN(bs)              ((bs)->view.length)
#define VIEW_GET(bs)                ((bs)->view)

using namespace argon::vm::datatype;

ArObject *trim(Bytes *self, Dict *kwargs, bool left, bool right) {
    const unsigned char *trim_buffer = nullptr;
    ArSize trim_length = 0;

    Bytes *tmp = nullptr;

    if (kwargs != nullptr) {
        if (!DictLookup(kwargs, "chars", (ArObject **) &tmp))
            return nullptr;

        if (tmp != nullptr) {
            if (!AR_TYPEOF(tmp, type_bytes_)) {
                Release(tmp);

                ErrorFormat(kTypeError[0], kTypeError[2], type_bytes_->qname, AR_TYPE_QNAME(tmp));

                return nullptr;
            }

            tmp->lock();

            trim_buffer = BUFFER_GET(tmp);
            trim_length = BUFFER_LEN(tmp);
        }
    }

    auto *ret = BytesTrim(self, trim_buffer, trim_length, left, right);

    if (tmp != nullptr) {
        tmp->unlock();

        Release(tmp);
    }

    return (ArObject *) ret;
}

ARGON_FUNCTION(bytes_bytes, Bytes,
               "Creates bytes object.\n"
               "\n"
               "The src parameter is optional, in case of call without src parameter an empty zero-length "
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

    std::shared_lock _(*self);

    if (BUFFER_LEN(self) == 0 || toupper(*BUFFER_GET(self)) == *BUFFER_GET(self))
        return (ArObject *) IncRef(self);

    if ((ret = BytesNew(BUFFER_GET(self), BUFFER_LEN(self), BUFFER_FROZEN(self))) == nullptr)
        return nullptr;

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

    if (BUFFER_FROZEN(self)) {
        ErrorFormat(kValueError[0], "frozen Bytes object cannot be used as destination for copy");
        return nullptr;
    }

    std::unique_lock _(*self);

    const auto *raw = BUFFER_GET(self);
    auto length = BUFFER_LEN(self);

    if (_self != src) {
        if (!BufferGet(src, &buffer, BufferFlags::READ))
            return nullptr;

        raw = buffer.buffer;
        length = buffer.length;
    }

    if (cplen > BUFFER_LEN(self) - doff)
        cplen = BUFFER_LEN(self) - doff;

    if (cplen > length - soff)
        cplen = length - soff;

    argon::vm::memory::MemoryCopy(BUFFER_GET(self) + doff, raw + soff, cplen);

    if (_self != src)
        BufferRelease(&buffer);

    _.unlock();

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

    std::shared_lock _(*self);

    count = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    count = stratum::util::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - count), buffer.buffer, count);

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

    std::shared_lock _(*self);

    res = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    res = stratum::util::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - res), buffer.buffer, res);

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

    std::shared_lock _(*self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length);

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
             "iu: offset, iu: byte", false, false) {
    auto *self = (Bytes *) _self;
    ArSize start = ((Integer *) args[0])->uint;
    auto pattern = (unsigned char) ((Integer *) args[1])->uint;

    if (pattern > 255) {
        ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

        return nullptr;
    }

    std::shared_lock _(*self);

    for (ArSize i = start; i < BUFFER_LEN(self); i++) {
        if (BUFFER_GET(self)[i] == pattern)
            return (ArObject *) IntNew((IntegerUnderlying) (i - start));
    }

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

    std::shared_lock _(*self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isalnum(BUFFER_GET(self)[i]))
            return BoolToArBool(false);
    }

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

    std::shared_lock _(*self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isalpha(BUFFER_GET(self)[i]))
            return BoolToArBool(false);
    }

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

    std::shared_lock _(*self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (BUFFER_GET(self)[i] > 0x7F)
            return BoolToArBool(false);
    }

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

    std::shared_lock _(*self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isdigit(BUFFER_GET(self)[i]))
            return BoolToArBool(false);
    }

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

    std::shared_lock _(*self);

    for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
        if (!std::isdigit(BUFFER_GET(self)[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD(bytes_isfrozen, isfrozen,
             "Check if this bytes object is frozen.\n"
             "\n"
             "- Returns: True if it is frozen, false otherwise.\n",
             nullptr, false, false) {
    return BoolToArBool(BUFFER_FROZEN((Bytes *) _self));
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

    std::shared_lock _(*self);

    while ((item = IteratorNext(iter)) != nullptr) {
        const unsigned char *item_buf = BUFFER_GET(self);
        len = BUFFER_LEN(self);

        if (item != _self) {
            if (!BufferGet(item, &buffer, BufferFlags::READ))
                goto ERROR;

            item_buf = buffer.buffer;
            len = buffer.length;
        }

        if (idx > 0 && !BufferViewAppendData(&ret->view, BUFFER_GET(self), BUFFER_LEN(self)))
            goto ERROR;

        if (!BufferViewAppendData(&ret->view, item_buf, len))
            goto ERROR;

        if (item != _self)
            BufferRelease(&buffer);

        Release(item);

        idx++;
    }

    Release(iter);

    return (ArObject *) ret;

    ERROR:
    _.unlock();

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
    auto *self = (Bytes *) _self;
    Bytes *ret;

    std::shared_lock _(*self);

    if ((ret = BytesNew(BUFFER_GET(self), BUFFER_LEN(self), BUFFER_FROZEN(self))) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < BUFFER_LEN(ret); i++)
        BUFFER_GET(ret)[i] = (unsigned char) tolower(BUFFER_GET(ret)[i]);

    return (ArObject *) ret;
}

ARGON_METHOD(bytes_ltrim, ltrim,
             "Returns a new bytes string stripped of whitespace from left ends.\n"
             "\n"
             "- KWParameters:\n"
             "  - chars: A set of characters to remove as leading characters.\n"
             "- Returns: New bytes string without whitespace.\n",
             nullptr, false, true) {
    return trim((Bytes *) _self, (Dict *) kwargs, true, false);
}

ARGON_METHOD(bytes_replace, replace,
             "Returns new bytes string where a specified value is replaced with a specified value.\n"
             "\n"
             "- Parameters:\n"
             "  - old: Bytes string to search for.\n"
             "  - new: Bytes string to replace the old value with.\n"
             "- KWParameters:\n"
             "  - count: Number specifying how many occurrences of the old value you want to replace. "
             "To replace all occurrence use -1.\n"
             "- Returns: Bytes string where a specified value is replaced.\n",
             "x: old, x: new", false, true) {
    IntegerUnderlying count;

    if (!KParamLookupInt((Dict *) kwargs, "count", &count, -1))
        return nullptr;

    return (ArObject *) BytesReplace((Bytes *) _self, (Bytes *) args[0],
                                     (Bytes *) args[1], count);
}

ARGON_METHOD(bytes_reverse, reverse,
             "Create a new bytes string by reversing all bytes.\n"
             "\n"
             "- Returns: Reversed bytes string.\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;
    unsigned char *buffer;
    ArSize index = 0;

    std::shared_lock _(*self);

    if ((buffer = (unsigned char *) argon::vm::memory::Alloc(BUFFER_LEN(self))) == nullptr)
        return nullptr;

    for (ArSize i = BUFFER_LEN(self); i > 0; i--)
        buffer[index++] = BUFFER_GET(self)[i - 1];

    _.unlock();

    auto *ret = BytesNewHoldBuffer(buffer, BUFFER_LEN(self), BUFFER_LEN(self), false);
    if (ret == nullptr) {
        argon::vm::memory::Free(buffer);

        return nullptr;
    }

    return (ArObject *) ret;
}

ARGON_METHOD(bytes_rfind, rfind,
             "Searches the bytes string for a specified value and returns the last position of where it was found.\n"
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

    std::shared_lock _(*self);

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length, true);

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

    std::shared_lock _(*self);

    len = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);

    compare = argon::vm::memory::MemoryCompare(BUFFER_GET(self) + (BUFFER_LEN(self) - len), buffer.buffer, len);

    BufferRelease(&buffer);

    if (compare == 0)
        return (ArObject *) BytesNew(BUFFER_GET(self), BUFFER_LEN(self) - len, BUFFER_FROZEN(self));

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

    std::shared_lock _(*self);

    len = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);

    compare = argon::vm::memory::MemoryCompare(BUFFER_GET(self), buffer.buffer, len);

    BufferRelease(&buffer);

    if (compare == 0)
        return (ArObject *) BytesNew(BUFFER_GET(self) + len, BUFFER_LEN(self) - len, BUFFER_FROZEN(self));

    return (ArObject *) IncRef(self);
}

ARGON_METHOD(bytes_rtrim, rtrim,
             "Returns a new bytes string stripped of whitespace from right ends.\n"
             "\n"
             "- KWParameters:\n"
             "  - chars: A set of characters to remove as trailing characters.\n"
             "- Returns: New bytes string without whitespace.\n",
             nullptr, false, true) {
    return trim((Bytes *) _self, (Dict *) kwargs, false, true);
}

ARGON_METHOD(bytes_split, split,
             "Splits the bytes string at the specified separator and returns a list.\n"
             "\n"
             "- Parameters:\n"
             "  - pattern: Specifies the separator to use when splitting the bytes string.\n"
             "- KWParameters:\n"
             "  - splits: Specifies how many splits to do.\n"
             "- Returns: New list of bytes string.\n",
             ": pattern", false, true) {
    ArBuffer buffer{};
    const unsigned char *pattern = nullptr;
    ArObject *ret;

    ArSize plen = 0;
    IntegerUnderlying maxsplit;

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
            BufferRelease(&buffer);

            ErrorFormat(kValueError[0], "empty separator");
            return nullptr;
        }
    }

    if (!KParamLookupInt((Dict *) kwargs, "splits", &maxsplit, -1)) {
        BufferRelease(&buffer);

        return nullptr;
    }

    std::shared_lock _(*((Bytes *) _self));

    ret = support::Split(BUFFER_GET((Bytes *) _self),
                         pattern,
                         (support::SplitChunkNewFn<Bytes>) BytesNew,
                         BUFFER_LEN((Bytes *) _self),
                         plen,
                         maxsplit);

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD(bytes_splitlines, splitlines,
             "Splits the bytes string at the new line and returns a list.\n"
             "\n"
             "- KWParameters:\n"
             "  - splits: Specifies how many splits to do.\n"
             "- Returns: New list of bytes string.\n",
             nullptr, false, true) {
    IntegerUnderlying maxsplit;

    if (!KParamLookupInt((Dict *) kwargs, "splits", &maxsplit, -1))
        return nullptr;

    std::shared_lock _(*((Bytes *) _self));

    return support::SplitLines(
            BUFFER_GET((Bytes *) _self),
            (support::SplitChunkNewFn<Bytes>) BytesNew,
            BUFFER_LEN((Bytes *) _self),
            maxsplit);
}

ARGON_METHOD(bytes_splitws, splitws,
             "Splits the bytes string at the whitespace and returns a list.\n"
             "\n"
             "- KWParameters:\n"
             "  - splits: Specifies how many splits to do.\n"
             "- Returns: New list of bytes string.\n",
             nullptr, false, true) {
    IntegerUnderlying maxsplit;

    if (!KParamLookupInt((Dict *) kwargs, "splits", &maxsplit, -1))
        return nullptr;

    std::shared_lock _(*((Bytes *) _self));

    return support::Split(BUFFER_GET((Bytes *) _self),
                          nullptr,
                          (support::SplitChunkNewFn<Bytes>) BytesNew,
                          BUFFER_LEN((Bytes *) _self),
                          0,
                          maxsplit);
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

    std::shared_lock _(*self);

    res = BUFFER_LEN(self) > buffer.length ? buffer.length : BUFFER_LEN(self);
    res = stratum::util::MemoryCompare(BUFFER_GET(self), buffer.buffer, res);

    _.unlock();

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

    std::shared_lock _(*self);

    builder.WriteHex(BUFFER_GET(self), BUFFER_LEN(self));

    _.unlock();

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

    std::shared_lock _(*self);

    builder.Write(BUFFER_GET(self), BUFFER_LEN(self), 0);

    _.unlock();

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ARGON_METHOD(bytes_trim, trim,
             "Returns a new bytes string stripped of whitespace from both ends.\n"
             "\n"
             "- KWParameters:\n"
             "  - chars: A set of characters to remove as leading/trailing characters.\n"
             "- Returns: New bytes string without whitespace.\n",
             nullptr, false, true) {
    return trim((Bytes *) _self, (Dict *) kwargs, true, true);
}

ARGON_METHOD(bytes_upper, upper,
             "Return a copy of the bytes string converted to uppercase.\n"
             "\n"
             "- Returns: New bytes string with all characters converted to uppercase.\n",
             nullptr, false, false) {
    auto *self = (Bytes *) _self;
    Bytes *ret;

    std::shared_lock _(*self);

    if ((ret = BytesNew(BUFFER_GET(self), BUFFER_LEN(self), BUFFER_FROZEN(self))) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < BUFFER_LEN(ret); i++)
        BUFFER_GET(ret)[i] = (unsigned char) toupper(BUFFER_GET(ret)[i]);

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
        bytes_ltrim,
        bytes_tohex,
        bytes_tostr,
        bytes_replace,
        bytes_reverse,
        bytes_rfind,
        bytes_rmpostfix,
        bytes_rmprefix,
        bytes_rtrim,
        bytes_split,
        bytes_splitlines,
        bytes_splitws,
        bytes_startswith,
        bytes_trim,
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

    std::shared_lock _(*self);

    if (idx < 0)
        idx = BUFFER_LEN(self) + idx;

    if (idx < BUFFER_LEN(self))
        ret = (ArObject *) IntNew(BUFFER_GET(self)[idx]);
    else
        ErrorFormat(kOverflowError[0], "%s index out of range (index: %d, length: %d)",
                    type_bytes_->name, idx, BUFFER_LEN(self));

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

    std::shared_lock _(*self);

    slice_len = BoundsIndex(bounds, BUFFER_LEN(self), &start, &stop, &step);

    if (step < 0) {
        if ((ret = BytesNew(slice_len, true, false, BUFFER_FROZEN(self))) == nullptr)
            return nullptr;

        for (ArSize i = 0; stop < start; start += step)
            BUFFER_GET(ret)[i++] = BUFFER_GET(self)[start];
    } else
        ret = BytesNew(self, start, slice_len);

    return (ArObject *) ret;
}

ArObject *bytes_item_in(Bytes *self, ArObject *value) {
    ArBuffer buffer{};
    ArSSize index;

    if (!IsBufferable(value) && !AR_TYPEOF(value, type_int_) && !AR_TYPEOF(value, type_uint_)) {
        ErrorFormat(kTypeError[0],
                    "expected bufferable type/'%s'/'%s' got '%s'",
                    type_int_->name,
                    type_uint_->name,
                    AR_TYPE_NAME(value));

        return nullptr;
    }

    if (self == (Bytes *) value)
        return BoolToArBool(true);

    std::shared_lock _(*self);

    if (AR_TYPEOF(value, type_int_) || AR_TYPEOF(value, type_uint_)) {
        ArSize byte = ((Integer *) value)->sint;

        if (byte > 255) {
            ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

            return nullptr;
        }

        for (ArSize i = 0; i < BUFFER_LEN(self); i++) {
            if (BUFFER_GET(self)[i] == byte)
                return BoolToArBool(true);
        }

        return BoolToArBool(false);
    }

    if (!BufferGet(value, &buffer, BufferFlags::READ))
        return nullptr;

    index = support::Find(BUFFER_GET(self), BUFFER_LEN(self), buffer.buffer, buffer.length);

    BufferRelease(&buffer);

    return BoolToArBool(index >= 0);
}

ArSize bytes_length(const Bytes *self) {
    return BUFFER_LEN(self);
}

bool bytes_set_item(Bytes *self, ArObject *index, ArObject *value) {
    IntegerUnderlying idx;
    ArSize rvalue;

    if (BUFFER_FROZEN(self)) {
        ErrorFormat(kTypeError[0], "unable to set item to frozen %s object", type_bytes_->name);
        return false;
    }

    if (!AR_TYPEOF(index, type_int_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(index));
        return false;
    }

    idx = ((Integer *) index)->sint;

    if (AR_TYPEOF(value, type_int_) || AR_TYPEOF(value, type_uint_))
        rvalue = ((Integer *) value)->uint;
    else if (AR_TYPEOF(value, type_bytes_)) {
        auto *other = (Bytes *) value;

        std::shared_lock _(*other);

        if (BUFFER_LEN(other) > 1) {
            ErrorFormat(kValueError[0], "expected %s of length 1 not %d", type_bytes_->name, BUFFER_LEN(other));

            return false;
        }

        rvalue = BUFFER_GET(other)[0];
    } else {
        ErrorFormat(kTypeError[0], "expected %s or %s, found '%s'", type_uint_->name, type_bytes_->name,
                    AR_TYPE_NAME(index));

        return false;
    }

    if (rvalue > 255) {
        ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

        return false;
    }

    std::unique_lock _(*self);

    if (idx < 0)
        idx = BUFFER_LEN(self) + idx;

    if (idx < BUFFER_LEN(self))
        BUFFER_GET(self)[idx] = (unsigned char) rvalue;
    else
        ErrorFormat(kOverflowError[0], "bytes index out of range (index: %d, length: %d)", idx, BUFFER_LEN(self));

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
    bool shared = ENUMBITMASK_ISFALSE(flags, BufferFlags::WRITE);
    bool ok;

    shared ? self->lock_shared() : self->lock();

    ok = BufferSimpleFill((ArObject *) self, buffer, flags, BUFFER_GET(self), 1, BUFFER_LEN(self),
                          !BUFFER_FROZEN(self));

    if (!ok)
        shared ? self->unlock_shared() : self->unlock();

    return ok;
}

void bytes_rel_buffer(ArBuffer *buffer) {
    const auto *self = (Bytes *) buffer->object;

    ENUMBITMASK_ISTRUE(buffer->flags, BufferFlags::WRITE) ? self->unlock() : self->unlock_shared();
}

const BufferSlots bytes_buffer = {
        (BufferGetFn) bytes_get_buffer,
        bytes_rel_buffer
};

ArObject *bytes_add(Bytes *left, Bytes *right) {
    if (AR_TYPEOF(left, type_bytes_) && AR_SAME_TYPE(left, right))
        return (ArObject *) BytesConcat(left, right);

    if (!AR_TYPEOF(right, type_int_) && !AR_TYPEOF(right, type_uint_))
        return nullptr;

    const auto *integer = (Integer *) right;

    if (integer->uint > 255) {
        ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

        return nullptr;
    }

    std::shared_lock _(*left);

    Bytes *ret;
    if ((ret = BytesNew(BUFFER_LEN(left) + 1, false, false, BUFFER_FROZEN(left))) != nullptr) {
        if (BUFFER_GET(left) != nullptr) {
            argon::vm::memory::MemoryCopy(BUFFER_GET(ret), BUFFER_GET(left), BUFFER_LEN(left));
            BUFFER_LEN(ret) = BUFFER_LEN(left);
        }

        *(BUFFER_GET(ret) + BUFFER_LEN(ret)) = (char) integer->sint;
        BUFFER_LEN(ret)++;
    }

    return (ArObject *) ret;
}

ArObject *bytes_mod(Bytes *left, ArObject *args) {
    unsigned char *buffer;
    Bytes *ret;

    ArSize out_length;
    ArSize out_cap;

    std::shared_lock _(*left);

    StringFormatter fmt((const char *) BUFFER_GET(left), BUFFER_LEN(left), args, true);

    if ((buffer = fmt.Format(&out_length, &out_cap)) == nullptr) {
        auto *err = fmt.GetError();

        argon::vm::Panic(err);

        Release(err);

        return nullptr;
    }

    _.unlock();

    if ((ret = BytesNew(0, false, false, false)) != nullptr) {
        BUFFER_GET(ret) = buffer;

        ret->view.length = out_length;
        ret->view.shared->capacity = out_cap;

        fmt.ReleaseOwnership();
    }

    return (ArObject *) ret;
}

ArObject *bytes_mul(const Bytes *left, const ArObject *right) {
    const auto *l = left;
    UIntegerUnderlying times;

    // int * bytes -> bytes * int
    if (!AR_TYPEOF(left, type_bytes_)) {
        l = (const Bytes *) right;
        right = (const ArObject *) left;
    }

    if (!AR_TYPEOF(right, type_int_) && !AR_TYPEOF(right, type_uint_))
        return nullptr;

    times = ((const Integer *) right)->uint;

    if (AR_TYPEOF(right, type_int_)) {
        if (((const Integer *) right)->sint < 0) {
            ErrorFormat(kValueError[0], "bytes string cannot be multiplied by a negative value");
            return nullptr;
        }

        times = ((const Integer *) right)->sint;
    }

    std::shared_lock _(*l);

    Bytes *ret;
    if ((ret = BytesNew(BUFFER_LEN(l) * times, true, false, BUFFER_FROZEN(left))) != nullptr) {
        while (times--)
            argon::vm::memory::MemoryCopy(BUFFER_GET(ret) + (BUFFER_LEN(l) * times), BUFFER_GET(l), BUFFER_LEN(l));
    }

    return (ArObject *) ret;
}

ArObject *bytes_inp_add(Bytes *self, ArObject *right) {
    if (!AR_TYPEOF(self, type_bytes_))
        return nullptr;

    if (BUFFER_FROZEN(self))
        return bytes_add(self, (Bytes *) right);

    if (AR_TYPEOF(right, type_int_) || AR_TYPEOF(right, type_uint_)) {
        const auto *integer = (Integer *) right;
        auto byte = (unsigned char) 0;

        if (integer->uint > 255) {
            ErrorFormat(kValueError[0], "byte must be in range(0, 255)");

            return nullptr;
        }

        byte = (unsigned char) integer->sint;

        if (!BufferViewAppendData(&VIEW_GET(self), &byte, 1))
            return nullptr;
    } else if (AR_TYPEOF(right, type_bytes_)) {
        if (!BufferViewAppendData(&VIEW_GET(self), &VIEW_GET((Bytes *) right)))
            return nullptr;
    } else
        return nullptr;

    return (ArObject *) self;
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
        (BinaryOp) bytes_inp_add,
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

    std::shared_lock l_self(*self);
    std::shared_lock l_o(*o);

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

    std::shared_lock _(*self);

    builder.Write((const unsigned char *) "b\"", 2, BUFFER_LEN(self) + 1);
    builder.WriteEscaped(BUFFER_GET(self), BUFFER_LEN(self), 1);
    builder.Write((const unsigned char *) "\"", 1, 0);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ArSize bytes_hash(Bytes *self) {
    if (!BUFFER_FROZEN(self)) {
        ErrorFormat(kUnhashableError[0], "unable to hash unfrozen %s object", type_bytes_->name);
        return 0;
    }

    if (self->hash == 0) {
        self->hash = HashBytes(BUFFER_GET(self), BUFFER_LEN(self));
        self->hash = AR_NORMALIZE_HASH(self->hash);
    }

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
    std::shared_lock l_left(*left);
    std::shared_lock l_right(*right);

    Bytes *ret;

    if ((ret = BytesNew(BUFFER_LEN(left) + BUFFER_LEN(right), true, false, BUFFER_FROZEN(left))) != nullptr) {
        memory::MemoryCopy(BUFFER_GET(ret), BUFFER_GET(left), BUFFER_LEN(left));
        memory::MemoryCopy(BUFFER_GET(ret) + BUFFER_LEN(left), BUFFER_GET(right), BUFFER_LEN(right));
    }

    return ret;
}

Bytes *argon::vm::datatype::BytesFreeze(Bytes *bytes) {
    Bytes *ret;

    if (BUFFER_FROZEN(bytes))
        return IncRef(bytes);

    std::shared_lock _(*bytes);

    if ((ret = BytesNew(BUFFER_GET(bytes), BUFFER_LEN(bytes), true)) == nullptr)
        return nullptr;

    bytes_hash(ret);

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
        if (!BufferViewInit(&bs->view, cap, frozen)) {
            Release(bs);
            return nullptr;
        }

        if (same_len)
            bs->view.length = cap;

        if (fill_zero)
            memory::MemoryZero(BUFFER_GET(bs), cap);

        bs->hash = 0;
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
    }

    return bs;
}

Bytes *argon::vm::datatype::BytesNewHoldBuffer(unsigned char *buffer, ArSize cap, ArSize len, bool frozen) {
    auto *bs = MakeObject<Bytes>(type_bytes_);

    if (bs != nullptr) {
        if (!BufferViewHoldBuffer(&bs->view, buffer, len, cap, frozen)) {
            Release(bs);
            return nullptr;
        }

        bs->hash = 0;
    }

    return bs;
}

Bytes *argon::vm::datatype::BytesReplace(Bytes *bytes, Bytes *old, Bytes *nval, ArSSize n) {
    std::shared_lock l_bytes(*bytes);
    std::shared_lock l_old(*old);

    unsigned char *buffer;
    unsigned char *cursor;

    ArSize idx = 0;
    ArSize newsz;

    if (Equal((const ArObject *) bytes, (const ArObject *) old) || n == 0)
        return IncRef(bytes);

    // Compute replacements
    n = support::Count(BUFFER_GET(bytes), BUFFER_LEN(bytes), BUFFER_GET(old), BUFFER_LEN(old), n);
    if (n == 0)
        return IncRef(bytes);

    std::shared_lock l_nval(*nval);

    newsz = (BUFFER_LEN(bytes) + n * (BUFFER_LEN(nval) - BUFFER_LEN(old)));

    // Allocate buffer
    if ((buffer = (unsigned char *) argon::vm::memory::Alloc(newsz)) == nullptr)
        return nullptr;

    cursor = buffer;

    long match;
    while ((match = support::Find(BUFFER_GET(bytes) + idx, BUFFER_LEN(bytes) - idx,
                                  BUFFER_GET(old), BUFFER_LEN(old))) > -1) {
        cursor = (unsigned char *) argon::vm::memory::MemoryCopy(cursor, BUFFER_GET(bytes) + idx, match);

        idx += match + BUFFER_LEN(old);

        cursor = (unsigned char *) argon::vm::memory::MemoryCopy(cursor, BUFFER_GET(nval), BUFFER_LEN(nval));

        if (n > -1) {
            n--;
            if (n == 0)
                break;
        }
    }

    argon::vm::memory::MemoryCopy(cursor, BUFFER_GET(bytes) + idx, BUFFER_LEN(bytes) - idx);

    auto *ret = BytesNewHoldBuffer(buffer, newsz, newsz, true);
    if (ret == nullptr)
        argon::vm::memory::Free(buffer);

    return ret;
}

Bytes *argon::vm::datatype::BytesTrim(Bytes *bytes, const unsigned char *buffer, ArSize length, bool left, bool right) {
    auto *to_trim = (const unsigned char *) "\x09\x20";
    ArSize trim_length = 2;
    ArSize start = 0;
    ArSize end;
    ArSize i;

    std::shared_lock _(*bytes);

    end = BUFFER_LEN(bytes);

    if (buffer != nullptr && length > 0) {
        to_trim = buffer;
        trim_length = length;
    }

    i = 0;
    while (left && i < trim_length && start < end) {
        if (BUFFER_GET(bytes)[start] == to_trim[i]) {
            while (BUFFER_GET(bytes)[start] == to_trim[i])
                start++;

            i = 0;

            continue;
        }

        i++;
    }

    if (start == end)
        return BytesNew(0, true, false, true);

    i = 0;
    while (right && i < trim_length && end > 0) {
        if (BUFFER_GET(bytes)[end - 1] == to_trim[i]) {
            while (end > 0 && BUFFER_GET(bytes)[end - 1] == to_trim[i])
                end--;

            i = 0;

            continue;
        }

        i++;
    }

    if (start == 0 && end == BUFFER_LEN(bytes))
        return IncRef(bytes);

    return BytesNew(bytes, start, end - start);
}

// BYTES ITERATOR

ArObject *bytesiterator_iter_next(BytesIterator *self) {
    unsigned char byte;

    std::unique_lock iter_lock(self->lock);

    std::shared_lock _(*self->iterable);

    if (!self->reverse) {
        if (self->index < BUFFER_LEN(self->iterable)) {
            byte = *(BUFFER_GET(self->iterable) + self->index);

            self->index++;

            return (ArObject *) IntNew(byte);
        }

        return nullptr;
    }

    if (BUFFER_LEN(self->iterable) - self->index == 0)
        return nullptr;

    byte = *(BUFFER_GET(self->iterable) + (BUFFER_LEN(self->iterable) - self->index));

    self->index++;

    return (ArObject *) IntNew(byte);
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