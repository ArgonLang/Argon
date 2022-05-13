// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>

#include <memory/memory.h>
#include <vm/runtime.h>

#include <object/datatype/support/formatter.h>

#include "error.h"
#include "hash_magic.h"
#include "bool.h"
#include "bounds.h"
#include "integer.h"
#include "iterator.h"
#include "map.h"
#include "string.h"

using namespace argon::memory;
using namespace argon::object;

static Map *intern = nullptr;

ArSize argon::object::StringSubStrLen(const String *str, ArSize offset, ArSize graphemes) {
    const unsigned char *buf = str->buffer + offset;
    const unsigned char *end = str->buffer + str->len;

    if (graphemes == 0)
        return 0;

    while (graphemes-- && buf < end) {
        if (*buf >> 7u == 0x0)
            buf += 1;
        else if (*buf >> 5u == 0x6)
            buf += 2;
        else if (*buf >> 4u == 0xE)
            buf += 3;
        else if (*buf >> 3u == 0x1E)
            buf += 4;
    }

    return buf - (str->buffer + offset);
}

// STRING ITERATOR

bool str_iter_has_next(Iterator *self) {
    if (self->reversed)
        return self->index > 0;

    return self->index < ((String *) self->obj)->len;
}

ArObject *str_iter_next(Iterator *self) {
    unsigned char *buf = ((String *) self->obj)->buffer + self->index;
    ArObject *ret = nullptr;

    int len = 1;

    if (!str_iter_has_next(self))
        return nullptr;

    if (!self->reversed)
        len = StringSubStrLen((String *) self->obj, self->index, 1);
    else {
        buf--;
        while (buf > ((String *) self->obj)->buffer && *buf >> 6 == 0x2) {
            buf--;
            len++;
        }
    }

    if ((ret = StringIntern((const char *) buf, len)) != nullptr)
        self->index += self->reversed ? -len : len;

    return ret;
}

ArObject *str_iter_peek(Iterator *self) {
    auto idx = self->index;
    auto ret = str_iter_next(self);

    self->index = idx;

    return ret;
}

ITERATOR_NEW(str_iterator, str_iter_next, str_iter_peek);

String *StringInit(ArSize len, bool mkbuf) {
    auto str = ArObjectNew<String>(RCType::INLINE, type_string_);

    if (str != nullptr) {
        str->buffer = nullptr;

        if (mkbuf) {
            // +1 is '\0'
            if ((str->buffer = (unsigned char *) Alloc(len + 1)) == nullptr) {
                argon::vm::Panic(error_out_of_memory);
                Release(str);
                return nullptr;
            }

            // Set terminator
            str->buffer[(len + 1) - 1] = 0x00;
        }

        str->kind = StringKind::ASCII;
        str->intern = false;
        str->len = len;
        str->cp_len = 0;
        str->hash = 0;
    }

    return str;
}

int argon::object::StringCompare(String *self, String *other) {
    ArSize idx1 = 0;
    ArSize idx2 = 0;

    unsigned char c1;
    unsigned char c2;

    do {
        c1 = self->buffer[idx1++];
        c2 = other->buffer[idx2++];

        // Take care of '\0', String->len not include '\0'
    } while (c1 == c2 && idx1 < self->len + 1);

    return c1 - c2;
}

ArSize FillBuffer(String *dst, ArSize offset, const unsigned char *buf, ArSize len) {
    StringKind kind = StringKind::ASCII;
    ArSize idx = 0;
    ArSize uidx = 0;

    while (idx < len) {
        dst->buffer[idx + offset] = buf[idx];

        if (buf[idx] >> 7u == 0x0)
            uidx += 1;
        else if (buf[idx] >> 5u == 0x6) {
            kind = StringKind::UTF8_2;
            uidx += 2;
        } else if (buf[idx] >> 4u == 0xE) {
            kind = StringKind::UTF8_3;
            uidx += 3;
        } else if (buf[idx] >> 3u == 0x1E) {
            kind = StringKind::UTF8_4;
            uidx += 4;
        }

        if (++idx == uidx)
            dst->cp_len++;

        if (kind > dst->kind)
            dst->kind = kind;
    }

    return idx;
}

bool string_get_buffer(String *self, ArBuffer *buffer, ArBufferFlags flags) {
    return BufferSimpleFill(self, buffer, flags, self->buffer, 1, self->len, false);
}

const BufferSlots string_buffer = {
        (BufferGetFn) string_get_buffer,
        nullptr
};

ArObject *string_add(ArObject *left, ArObject *right) {
    if (left->type == right->type && left->type == type_string_)
        return StringConcat((String *) left, (String *) right);

    return nullptr;
}

ArObject *string_mul(ArObject *left, ArObject *right) {
    auto l = (String *) left;
    String *ret = nullptr;

    IntegerUnderlying times;

    // int * str -> str * int
    if (left->type != type_string_) {
        l = (String *) right;
        right = left;
    }

    if (right->type == type_integer_) {
        times = ((Integer *) right)->integer;
        if ((ret = StringInit(l->len * times, true)) != nullptr) {
            while (times--)
                FillBuffer(ret, l->len * times, l->buffer, l->len);
        }
    }

    return ret;
}

const OpSlots string_ops{
        string_add,
        nullptr,
        string_mul,
        nullptr,
        nullptr,
        (BinaryOp) argon::object::StringFormat,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        string_add,
        nullptr,
        string_mul,
        nullptr,
        nullptr,
        nullptr
};

ArSize argon::object::StringLen(const String *str) {
    return str->len;
}

ArObject *string_get_item(String *self, ArSSize index) {
    String *ret;

    if (self->kind != StringKind::ASCII)
        return ErrorFormat(type_unicode_index_error_, "unable to index a unicode string");

    if (index < 0)
        index = self->len + index;

    if (index >= self->len)
        return ErrorFormat(type_overflow_error_, "string index out of range (len: %d, idx: %d)", self->len, index);

    ret = StringIntern((const char *) self->buffer + index, 1);

    return ret;
}

ArObject *string_get_slice(String *self, ArObject *bounds) {
    auto b = (Bounds *) bounds;
    String *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    if (self->kind != StringKind::ASCII)
        return ErrorFormat(type_unicode_index_error_, "unable to slice a unicode string");

    slice_len = BoundsIndex(b, self->len, &start, &stop, &step);

    if ((ret = StringInit(slice_len, true)) == nullptr)
        return nullptr;

    ret->cp_len = slice_len;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step)
            ret->buffer[i++] = self->buffer[start];
    } else {
        for (ArSize i = 0; stop < start; start += step)
            ret->buffer[i++] = self->buffer[start];
    }

    return ret;
}

const SequenceSlots string_sequence = {
        (SizeTUnaryOp) StringLen,
        (BinaryOpArSize) string_get_item,
        nullptr,
        (BinaryOp) string_get_slice,
        nullptr
};

ARGON_FUNCTION5(str_, new,
                "Create a new string object from the given object."
                ""
                "- Parameter [obj]: specifies the object to convert into a string."
                "- Returns: new string.", 0, true) {
    if (!VariadicCheckPositional("str::new", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (AR_TYPEOF(*argv, type_string_))
            return IncRef(*argv);

        return ToString(*argv);
    }

    return StringIntern((char *) "");
}

ARGON_METHOD5(str_, capitalize,
              "Return a capitalized version of the string."
              ""
              "- Returns: new capitalized string.", 0, false) {
    auto *base = (String *) self;
    String *ret;

    if (base->len == 0 || toupper(*base->buffer) == *base->buffer)
        return IncRef(base);

    if ((ret = StringNew((const char *) base->buffer, base->len)) == nullptr)
        return nullptr;

    *ret->buffer = toupper(*ret->buffer);

    return ret;
}

ARGON_FUNCTION5(str_, chr,
                "Returns the character that represents the specified unicode."
                ""
                "- Parameter number: an integer representing a valid Unicode code point."
                "- Returns: new string that contains the specified character.", 1, false) {
    unsigned char buf[] = {0x00, 0x00, 0x00, 0x00};
    ArObject *ret;
    ArSize len;

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "chr expected an integer not '%s'", AR_TYPE_NAME(argv[0]));

    len = StringIntToUTF8(((Integer *) argv[0])->integer, buf);

    if ((ret = StringNew((const char *) buf, len)) == nullptr)
        return nullptr;

    return ret;
}

ARGON_METHOD5(str_, count,
              "Returns the number of times a specified value occurs in a string."
              ""
              "- Parameter str: The string to value to search for."
              "- Returns: number of times a specified value appears in the string.", 1, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];
    ArSSize n;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::count() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    n = support::Count(str->buffer, str->len, pattern->buffer, pattern->len, -1);

    return IntegerNew(n);
}

ARGON_METHOD5(str_, endswith,
              "Returns true if the string ends with the specified value."
              ""
              "- Parameter str: The value to check if the string ends with."
              "- Returns: true if the string ends with the specified value, false otherwise."
              ""
              "# SEE"
              "- startswith: Returns true if the string starts with the specified value.", 1, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::endswith() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    return BoolToArBool(StringEndsWith(str, pattern));
}

ARGON_METHOD5(str_, find,
              "Searches the string for a specified value and returns the position of where it was found."
              ""
              "- Parameter str: The value to search for."
              "- Returns: index of the first position, -1 otherwise."
              ""
              "# SEE"
              "- rfind: Same as find, but returns the index of the last position.", 1, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];
    ArSSize n;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::find() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    n = StringFind(str, pattern);

    return IntegerNew(n);
}

ARGON_METHOD5(str_, lower,
              "Return a copy of the string converted to lowercase."
              ""
              "- Returns: new string with all characters converted to lowercase.", 0, false) {
    ArSize len = ((String *) self)->len;
    String *ret;
    unsigned char *buf;

    if ((buf = ArObjectNewRaw<unsigned char *>(len + 1)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < len; i++)
        buf[i] = tolower(((String *) self)->buffer[i]);

    buf[len] = '\0';

    if ((ret = StringNewBufferOwnership(buf, len)) == nullptr) {
        Free(buf);
        return nullptr;
    }

    return ret;
}

ARGON_METHOD5(str_, replace,
              "Returns a string where a specified value is replaced with a specified value."
              ""
              "- Parameters:"
              " - old: the string to search for."
              " - new: the string to replace the old value with."
              " - count: A number specifying how many occurrences of the old value you want to replace."
              " To replace all occurrence use count = -1."
              "- Returns: string where a specified value is replaced.", 3, false) {
    auto *str = (String *) self;
    ArSSize n;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::replace() first parameter expected string not '%s'",
                           AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_string_))
        return ErrorFormat(type_type_error_, "str::replace() second parameter expected string not '%s'",
                           AR_TYPE_NAME(argv[1]));

    if (!AR_TYPEOF(argv[2], type_integer_))
        return ErrorFormat(type_type_error_, "str::replace() third parameter expected integer not '%s'",
                           AR_TYPE_NAME(argv[2]));

    n = ((Integer *) argv[2])->integer;

    return StringReplace(str, (String *) argv[0], (String *) argv[1], n);
}

ARGON_METHOD5(str_, rfind,
              "Searches the string for a specified value and returns the last position of where it was found."
              ""
              "- Parameter str: The value to search for."
              "- Returns: index of the last position, -1 otherwise."
              ""
              "# SEE"
              "- find: Same as rfind, but returns the index of the first position.", 1, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];
    ArSSize n;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::rfind() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    n = StringRFind(str, pattern);

    return IntegerNew(n);
}

ARGON_METHOD5(str_, join,
              "Joins the elements of an iterable to the end of the string."
              ""
              "- Parameter iterable: Any iterable object where all the returned values are strings."
              "- Returns: new string where all items in an iterable are joined into one string.", 1, false) {
    StringBuilder builder;
    const auto *str = (String *) self;
    ArObject *iter;
    String *tmp;

    ArSize idx = 0;

    if (!CheckArgs("I:iterable", func, argv, count))
        return nullptr;

    if ((iter = IteratorGet(argv[0])) == nullptr)
        return nullptr;

    while ((tmp = (String *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_string_)) {
            Release(tmp);
            Release(iter);
            return ErrorFormat(type_type_error_, "sequence item %i: expected string not '%s'", idx, AR_TYPE_NAME(tmp));
        }

        if (idx > 0 && !builder.Write(str, (int) tmp->len)) {
            Release(tmp);
            Release(iter);
            return nullptr;
        }

        builder.Write(tmp, 0);

        Release(tmp);
        idx++;
    }

    Release(iter);
    return builder.BuildString();
}

ARGON_METHOD5(str_, split,
              "Splits the string at the specified separator, and returns a list."
              ""
              "- Parameters:"
              " - separator: specifies the separator to use when splitting the string."
              " - maxsplit: specifies how many splits to do."
              "- Returns: new list of string.", 2, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::split() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "str::split() expected integer not '%s'", AR_TYPE_NAME(argv[1]));

    return StringSplit(str, pattern, ((Integer *) argv[1])->integer);
}

ARGON_METHOD5(str_, startswith,
              "Returns true if the string starts with the specified value."
              ""
              "- Parameter str: The value to check if the string starts with."
              "- Returns: true if the string starts with the specified value, false otherwise."
              ""
              "# SEE"
              "- endswith: Returns true if the string ends with the specified value.", 1, false) {
    auto *str = (String *) self;
    auto *pattern = (String *) argv[0];
    ArSSize n;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "str::startswith() expected string not '%s'", AR_TYPE_NAME(argv[0]));

    n = str->len - pattern->len;
    if (n >= 0 && MemoryCompare(str->buffer, pattern->buffer, pattern->len) == 0)
        return IncRef(True);

    return IncRef(False);
}

ARGON_METHOD5(str_, trim,
              "Returns a new string stripped of whitespace from both ends."
              ""
              "- Returns: new string without whitespace.", 0, false) {
    auto *str = (String *) self;
    ArSize start = 0;
    ArSize end = str->len;

    while (str->buffer[start] == 0x09 || str->buffer[start] == 0x20)
        start++;

    while (str->buffer[end - 1] == 0x09 || str->buffer[end - 1] == 0x20)
        end--;

    return StringNew((const char *) str->buffer + start, end - start);
}

ARGON_METHOD5(str_, upper,
              "Return a copy of the string converted to uppercase."
              ""
              "- Returns: new string with all characters converted to uppercase.", 0, false) {
    ArSize len = ((String *) self)->len;
    String *ret;
    unsigned char *buf;

    if ((buf = ArObjectNewRaw<unsigned char *>(len + 1)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < len; i++)
        buf[i] = toupper(((String *) self)->buffer[i]);

    buf[len] = '\0';

    if ((ret = StringNewBufferOwnership(buf, len)) == nullptr) {
        Free(buf);
        return nullptr;
    }

    return ret;
}

const NativeFunc str_methods[] = {
        str_new_,
        str_capitalize_,
        str_count_,
        str_chr_,
        str_endswith_,
        str_find_,
        str_lower_,
        str_replace_,
        str_rfind_,
        str_join_,
        str_split_,
        str_startswith_,
        str_trim_,
        str_upper_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots str_obj = {
        str_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

bool string_is_true(String *self) {
    return self->len > 0;
}

ArObject *string_compare(String *self, ArObject *other, CompareMode mode) {
    auto *o = (String *) other;
    int left = 0;
    int right = 0;
    int res;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self != other) {
        if (mode == CompareMode::EQ && self->kind != o->kind)
            return BoolToArBool(false);

        res = StringCompare(self, (String *) other);
        if (res < 0)
            left = -1;
        else if (res > 0)
            right = -1;
    }

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArSize string_hash(String *self) {
    if (self->hash == 0)
        self->hash = HashBytes(self->buffer, self->len);

    return self->hash;
}

String *string_repr(String *self) {
    StringBuilder builder;

    builder.Write((const unsigned char *) "\"", 1, self->len + 1);
    builder.WriteEscaped(self->buffer, self->len, 1, true);
    builder.Write((const unsigned char *) "\"", 1, 0);

    return builder.BuildString();
}

String *string_str(String *self) {
    return IncRef(self);
}

ArObject *string_iter_get(String *self) {
    return IteratorNew(&type_str_iterator_, self, false);
}

ArObject *string_iter_rget(String *self) {
    return IteratorNew(&type_str_iterator_, self, true);
}

void string_cleanup(String *self) {
    argon::memory::Free(self->buffer);
}

const TypeInfo StringType = {
        TYPEINFO_STATIC_INIT,
        "string",
        nullptr,
        sizeof(String),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) string_cleanup,
        nullptr,
        (CompareOp) string_compare,
        (BoolUnaryOp) string_is_true,
        (SizeTUnaryOp) string_hash,
        (UnaryOp) string_repr,
        (UnaryOp) string_str,
        (UnaryOp) string_iter_get,
        (UnaryOp) string_iter_rget,
        &string_buffer,
        nullptr,
        nullptr,
        nullptr,
        &str_obj,
        &string_sequence,
        &string_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_string_ = &StringType;

String *argon::object::StringNew(const char *string, ArSize len) {
    auto str = StringInit(len, true);

    if (str != nullptr && string != nullptr)
        FillBuffer(str, 0, (unsigned char *) string, len);

    return str;
}

String *argon::object::StringNewBufferOwnership(unsigned char *buffer, ArSize len) {
    String *str;

    if (buffer == nullptr || len == 0) {
        memory::Free(buffer); // if buffer != nullptr, but len == 0
        return StringIntern("");
    }

    if ((str = StringInit(len, false)) != nullptr) {
        str->buffer = (unsigned char *) buffer;
        FillBuffer(str, 0, (unsigned char *) buffer, len);
    }

    return str;
}

String *argon::object::StringNewFormat(const char *string, va_list vargs) {
    va_list vargs2;
    String *str;
    int sz;

    va_copy(vargs2, vargs);
    sz = vsnprintf(nullptr, 0, string, vargs2) + 1; // +1 is for '\0'
    va_end(vargs2);

    if ((str = StringInit(sz - 1, true)) == nullptr)
        return nullptr;

    str->cp_len = sz - 1;

    va_copy(vargs2, vargs);
    vsnprintf((char *) str->buffer, sz, string, vargs2);
    va_end(vargs2);

    return str;
}

String *argon::object::StringNewFormat(const char *string, ...) {
    va_list args;
    String *str;

    va_start(args, string);
    str = StringNewFormat(string, args);
    va_end(args);

    return str;
}

String *argon::object::StringIntern(const char *string, ArSize len) {
    String *ret = nullptr;

    if (intern == nullptr) {
        intern = MapNew();
        // TODO: log!
    } else
        ret = (String *) MapGetFrmStr(intern, string, len);

    if (ret == nullptr) {
        if ((ret = StringNew(string, len)) != nullptr) {
            if (intern != nullptr) {
                if (MapInsert(intern, ret, ret))
                    ret->intern = true;
                // TODO: log if false!
            }
        }
    }

    return ret;
}

ArObject *StringSplitWhiteSpaces(const String *string, ArSSize maxsplit) {
    String *tmp;
    List *ret;
    ArSize cursor = 0;
    ArSSize start = -1;
    ArSize end;

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    end = string->len;

    if (maxsplit != 0)
        start = support::FindWhitespace(string->buffer, &end);

    while (start > -1 && (maxsplit == -1 || maxsplit > 0)) {
        tmp = StringNew((const char *) string->buffer + cursor, start);
        cursor += end;

        if (tmp == nullptr) {
            Release(ret);
            return nullptr;
        }

        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(ret);
            return nullptr;
        }

        Release(tmp);

        end = string->len - cursor;
        start = support::FindWhitespace(string->buffer + cursor, &end);

        if (maxsplit != -1)
            maxsplit--;
    }

    if (string->len - cursor > 0) {
        if ((tmp = StringNew((const char *) string->buffer + cursor, string->len - cursor)) == nullptr) {
            Release(ret);
            return nullptr;
        }

        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(ret);
            return nullptr;
        }
    }

    return ret;
}

ArObject *
argon::object::StringSplit(const String *string, const unsigned char *pattern, ArSize plen, ArSSize maxsplit) {
    String *tmp;
    List *ret;
    ArSize cursor = 0;
    ArSSize start;

    if (pattern == nullptr || plen == 0)
        return StringSplitWhiteSpaces(string, maxsplit);

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    start = support::Find(string->buffer, string->len, pattern, plen);
    while (start > -1 && (maxsplit == -1 || maxsplit > 0)) {
        tmp = StringNew((const char *) string->buffer + cursor, (cursor + start) - cursor);
        cursor += start + plen;

        if (tmp == nullptr) {
            Release(ret);
            return nullptr;
        }

        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(ret);
            return nullptr;
        }

        Release(tmp);

        start = support::Find(string->buffer + cursor, string->len, pattern, plen);
        if (maxsplit != -1)
            maxsplit--;
    }

    if (string->len - cursor > 0) {
        if ((tmp = StringNew((const char *) string->buffer + cursor, string->len - cursor)) == nullptr) {
            Release(ret);
            return nullptr;
        }

        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(ret);
            return nullptr;
        }
    }

    return ret;
}

bool argon::object::StringEndsWith(String *string, String *pattern) {
    ArSSize n;

    n = string->len - pattern->len;
    if (n >= 0 && MemoryCompare(string->buffer + n, pattern->buffer, pattern->len) == 0)
        return true;

    return false;
}

bool argon::object::StringEq(String *string, const unsigned char *c_str, ArSize len) {
    if (string->len != len)
        return false;

    for (ArSize i = 0; i < string->len; i++) {
        if (string->buffer[i] != c_str[i])
            return false;
    }

    return true;
}

int argon::object::StringIntToUTF8(unsigned int glyph, unsigned char *buf) {
    if (glyph < 0x80) {
        *buf = glyph >> 0u & 0x7Fu;
        return 1;
    } else if (glyph < 0x0800) {
        *buf++ = glyph >> 6u & 0x1Fu | 0xC0u;
        *buf = glyph >> 0u & 0xBFu;
        return 2;
    } else if (glyph < 0x010000) {
        *buf++ = glyph >> 12u & 0x0Fu | 0xE0u;
        *buf++ = glyph >> 6u & 0x3Fu | 0x80u;
        *buf = glyph >> 0u & 0x3Fu | 0x80u;
        return 3;
    } else if (glyph < 0x110000) {
        *buf++ = glyph >> 18u & 0x07u | 0xF0u;
        *buf++ = glyph >> 12u & 0x3Fu | 0x80u;
        *buf++ = glyph >> 6u & 0x3Fu | 0x80u;
        *buf = glyph >> 0u & 0x3Fu | 0x80u;
        return 4;
    }

    return 0;
}

int argon::object::StringUTF8toInt(const unsigned char *buf) {
    if (*buf > 0xF0)
        return -1;

    if ((*buf & 0xF0) == 0xF0)
        return (*buf & 0x07) << 21 | (buf[1] & 0x3F) << 12 | (buf[2] & 0x3F) << 6 | buf[3] & 0x3F;
    else if ((*buf & 0xE0) == 0xE0)
        return (*buf & 0x0F) << 12 | (buf[1] & 0x3F) << 6 | buf[2] & 0x3F;
    else if ((*buf & 0xC0) == 0xC0)
        return (*buf & 0x1F) << 6 | buf[1] & 0x3F;

    return *buf;
}

String *argon::object::StringConcat(String *left, String *right) {
    String *ret = StringInit(left->len + right->len, true);

    if (ret != nullptr) {
        memory::MemoryCopy(ret->buffer, left->buffer, left->len);
        memory::MemoryCopy(ret->buffer + left->len, right->buffer, right->len);

        ret->kind = left->kind;
        if (right->kind > left->kind)
            ret->kind = right->kind;
        ret->cp_len = left->cp_len + right->cp_len;
    }

    return ret;
}

String *argon::object::StringConcat(String *left, const char *right, bool internal) {
    String *ret = nullptr;
    String *astr;

    astr = internal ? StringIntern(right) : StringNew(right);

    if (astr != nullptr) {
        if ((ret = StringConcat(left, astr)) == nullptr) {
            Release(astr);
            return nullptr;
        }
    }

    Release(astr);
    return ret;
}

String *argon::object::StringCFormat(const char *fmt, ArObject *args) {
    unsigned char *buf;
    String *str;
    ArSize len;

    support::Formatter formatter(fmt, args);

    if ((buf = formatter.format(&len)) == nullptr)
        return nullptr;

    if ((str = StringNewBufferOwnership(buf, len)) == nullptr)
        return nullptr;

    formatter.ReleaseBufferOwnership();
    return str;
}

String *argon::object::StringFormat(String *fmt, ArObject *args) {
    unsigned char *buf;
    String *str;
    ArSize len;

    support::Formatter formatter((const char *) fmt->buffer, fmt->len, args);

    if ((buf = formatter.format(&len)) == nullptr)
        return nullptr;

    if ((str = StringNewBufferOwnership(buf, len)) == nullptr)
        return nullptr;

    formatter.ReleaseBufferOwnership();
    return str;
}

String *argon::object::StringReplace(String *string, String *old, String *nval, ArSSize n) {
    String *nstring;

    ArSize idx = 0;
    ArSize nidx = 0;
    ArSize newsz;

    if (Equal(string, old) || n == 0) {
        IncRef(string);
        return string;
    }

    // Compute replacements
    n = support::Count(string->buffer, string->len, old->buffer, old->len, n);

    newsz = (string->len + n * (nval->len - old->len));

    // Allocate string
    if ((nstring = StringInit(newsz, true)) == nullptr)
        return nullptr;

    long match;
    while ((match = support::Find(string->buffer + idx, string->len - idx, old->buffer, old->len)) > -1) {
        FillBuffer(nstring, nidx, string->buffer + idx, match);

        idx += match + old->len;
        nidx += match;

        FillBuffer(nstring, nidx, nval->buffer, nval->len);
        nidx += nval->len;

        if (n > -1 && --n == 0)
            break;
    }
    FillBuffer(nstring, nidx, string->buffer + idx, string->len - idx);

    return nstring;
}

String *argon::object::StringSubs(String *string, ArSize start, ArSize end) {
    String *ret;
    ArSize len;

    if (start >= string->len)
        return nullptr;

    if (end == 0 || end > string->len)
        end = string->len;

    if (start >= end)
        return nullptr;

    len = end - start;

    if (string->kind != StringKind::ASCII)
        len = StringSubStrLen(string, start, end - start);

    if ((ret = StringInit(len, true)) != nullptr)
        FillBuffer(ret, 0, string->buffer + start, len);

    return ret;
}

// StringBuilder

StringBuilder::~StringBuilder() {
    Free(this->buffer_);
}

ArSize StringBuilder::GetEscapedLength(const unsigned char *buffer, ArSize len, bool unicode) {
    ArSize str_len = 0;

    for (ArSize i = 0; i < len; i++) {
        switch (buffer[i]) {
            case '"':
            case '\\':
            case '\t':
            case '\n':
            case '\r':
                str_len += 2; // \C
                break;
            default:
                if (!unicode && (buffer[i] < ' ' || buffer[i] >= 0x7F)) {
                    str_len += 4;
                    break;
                }
                str_len++;
        }
    }

    return str_len;
}

bool StringBuilder::BufferResize(ArSize sz) {
    ArSize cap = this->cap_;
    unsigned char *tmp;

    if (this->error_)
        return false;

    if (cap > 0)
        cap--;

    if (sz == 0 || this->len_ + sz < cap)
        return true;

    if (this->buffer_ == nullptr)
        sz += 1;

    if ((tmp = ArObjectRealloc<unsigned char *>(this->buffer_, this->cap_ + sz)) == nullptr) {
        this->error_ = true;
        return false;
    }

    this->buffer_ = tmp;
    this->cap_ += sz;
    return true;
}

inline bool CheckUnicodeCharSequence(unsigned char chr, ArSize index, ArSize *uindex, StringKind *out_kind) {
    if (index == *uindex) {
        if (chr >> 7u == 0x0)
            *uindex += 1;
        else if (chr >> 5u == 0x6) {
            *out_kind = StringKind::UTF8_2;
            *uindex += 2;
        } else if (chr >> 4u == 0xE) {
            *out_kind = StringKind::UTF8_3;
            *uindex += 3;
        } else if (chr >> 3u == 0x1E) {
            *out_kind = StringKind::UTF8_4;
            *uindex += 4;
        } else if (chr >> 6u == 0x2) {
            ErrorFormat(type_unicode_error_, "can't decode byte 0x%x: invalid start byte", chr);
            return false;
        }
    } else if (chr >> 6u != 0x2) {
        ErrorFormat(type_unicode_error_, "can't decode byte 0x%x: invalid continuation byte", chr);
        return false;
    }

    return true;
}

bool StringBuilder::Write(const unsigned char *buffer, ArSize len, int overalloc) {
    StringKind kind = StringKind::ASCII;
    ArSize idx = 0;
    ArSize uidx = 0;
    unsigned char *wbuf;

    if (!this->BufferResize(len + overalloc))
        return false;

    if (len == 0)
        return true;

    wbuf = this->buffer_ + this->len_;

    while (idx < len) {
        wbuf[idx] = buffer[idx];

        if (!CheckUnicodeCharSequence(buffer[idx], idx, &uidx, &kind)) {
            this->error_ = true;
            return false;
        }

        if (++idx == uidx)
            this->cp_len_++;

        if (kind > this->kind_)
            this->kind_ = kind;
    }

    this->len_ += idx;
    return true;
}

bool StringBuilder::WriteEscaped(const unsigned char *buffer, ArSize len, int overalloc, bool unicode) {
    static const unsigned char hex[] = "0123456789abcdef";
    const unsigned char *start;
    unsigned char *buf;

    ArSize wlen = this->GetEscapedLength(buffer, len, unicode);

    if (!this->BufferResize(wlen + overalloc))
        return false;

    if (len == 0)
        return true;

    start = this->buffer_ + this->len_;
    buf = this->buffer_ + this->len_;

    for (ArSize i = 0; i < len; i++) {
        switch (buffer[i]) {
            case '"':
                *buf++ = '\\';
                *buf++ = '"';
                break;
            case '\\':
                *buf++ = '\\';
                *buf++ = '\\';
                break;
            case '\t':
                *buf++ = '\\';
                *buf++ = 't';
                break;
            case '\n':
                *buf++ = '\\';
                *buf++ = 'n';
                break;
            case '\r':
                *buf++ = '\\';
                *buf++ = 'r';
                break;
            default:
                if (!unicode && (buffer[i] < ' ' || buffer[i] >= 0x7F)) {
                    *buf++ = '\\';
                    *buf++ = 'x';
                    *buf++ = hex[(buffer[i] & 0xF0) >> 4];
                    *buf++ = hex[(buffer[i] & 0x0F)];
                    break;
                }

                *buf++ = buffer[i];
        }
    }

    this->len_ += buf - start;
    this->cp_len_ += buf - start;
    return len;
}

bool StringBuilder::WriteHex(const unsigned char *buffer, ArSize len) {
    static const unsigned char hex[] = "0123456789abcdef";
    unsigned char *buf;
    ArSize wlen = len * 4;

    if (!this->BufferResize(wlen))
        return false;

    if (len == 0)
        return true;

    buf = this->buffer_;

    for (ArSize i = 0; i < len; i++) {
        *buf++ = '\\';
        *buf++ = 'x';
        *buf++ = hex[(buffer[i] & 0xF0) >> 4];
        *buf++ = hex[(buffer[i] & 0x0F)];
    }

    this->len_ += wlen;
    this->cp_len_ += wlen;

    return true;
}

bool StringBuilder::WriteRepeat(char ch, int times) {
    if (!this->BufferResize(times))
        return false;

    for (int i = 0; i < times; i++)
        this->buffer_[this->len_++] = ch;

    this->cp_len_ += times;

    return true;
}

String *StringBuilder::BuildString() {
    String *str;

    if (this->error_)
        return nullptr;

    if (this->buffer_ == nullptr || this->len_ == 0)
        return StringIntern("");

    if ((str = StringInit(this->len_, false)) != nullptr) {
        this->buffer_[this->len_] = '\0';

        str->buffer = this->buffer_;
        this->buffer_ = nullptr;

        str->kind = this->kind_;
        str->cp_len = this->cp_len_;
    }

    return str;
}