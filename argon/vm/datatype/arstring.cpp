// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>
#include <cstdio>

#include <argon/vm/memory/memory.h>
#include <argon/vm/runtime.h>

#include <argon/vm/datatype/support/common.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/hash_magic.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/stringbuilder.h>
#include <argon/vm/datatype/stringformatter.h>

#include <argon/vm/datatype/arstring.h>

using namespace argon::vm::datatype;

#define STR_BUF(str) ((str)->buffer)
#define STR_LEN(str) ((str)->length)

static Dict *intern = nullptr;
static String *empty_string = nullptr;

bool StringInitKind(String *string) {
    Error *error;

    StringKind kind = StringKind::ASCII;
    ArSize index = 0;
    ArSize cp_length = 0;

    string->cp_length = 0;

    while (index < string->length) {
        if (!CheckUnicodeCharSequence(&kind, &cp_length, &error, string->buffer[index], index)) {
            argon::vm::Panic((ArObject *) error);

            Release(error);

            return false;
        }

        if (kind > string->kind)
            string->kind = kind;

        if (++index == cp_length)
            string->cp_length++;
    }

    return true;
}

String *StringInit(ArSize len, bool mkbuf) {
    auto str = MakeObject<String>(type_string_);

    if (str != nullptr) {
        str->buffer = nullptr;

        if (mkbuf) {
            // +1 is '\0'
            str->buffer = (unsigned char *) argon::vm::memory::Alloc(len + 1);
            if (str->buffer == nullptr) {
                Release(str);
                return nullptr;
            }

            // Set terminator
            STR_BUF(str)[(len + 1) - 1] = 0x00;
        }

        str->kind = StringKind::ASCII;
        str->intern = false;
        STR_LEN(str) = len;
        str->cp_length = 0;
        str->hash = 0;
    }

    return str;
}

bool string_get_buffer(String *self, ArBuffer *buffer, BufferFlags flags) {
    return BufferSimpleFill((ArObject *) self, buffer, flags, self->buffer, 1, self->length, false);
}

const BufferSlots string_buffer = {
        (BufferGetFn) string_get_buffer,
        nullptr
};

ARGON_FUNCTION(str_string, String,
             "Create a new string object from the given object.\n"
             "\n"
             "- Parameter obj: Object to convert into a string.\n"
             "- Returns: New string.\n",
             ": obj", false, false) {
    if (AR_TYPEOF(args[0], type_string_))
        return IncRef(args[0]);

    return Str(args[0]);
}

ARGON_METHOD(str_capitalize, capitalize,
             "Returns a capitalized version of the string. \n"
             "\n"
             "- Returns: New capitalized string.\n",
             nullptr, false, false) {
    auto *self = (String *) _self;
    String *ret;

    if (self->length == 0 || toupper(*self->buffer) == *self->buffer)
        return (ArObject *) IncRef(self);

    if ((ret = StringNew((const char *) STR_BUF(self), STR_LEN(self))) == nullptr)
        return nullptr;

    *ret->buffer = (unsigned char) toupper(*self->buffer);

    return (ArObject *) ret;
}

ARGON_FUNCTION(str_chr, chr,
               "Returns the character that represents the specified unicode.\n"
               "\n"
               "- Parameter num: Int/UInt representing a valid Unicode code point.\n"
               "- Returns: New string that contains the specified character.\n",
               "iu: num", false, false) {
    unsigned char buf[] = {0x00, 0x00, 0x00, 0x00};
    auto cp = (unsigned int) ((Integer *) args[0])->uint;
    ArSize len;

    if ((len = StringIntToUTF8(cp, buf)) == 0) {
        ErrorFormat(kUnicodeError[0], kUnicodeError[4], cp);
        return nullptr;
    }

    return (ArObject *) StringNew((const char *) buf, len);
}

ARGON_METHOD(str_count, count,
             "Returns the number of times a specified value occurs in a string.\n"
             "\n"
             "- Parameter pattern: The string to value to search for.\n"
             "- Returns: Number of times a specified value appears in the string.\n",
             "s: pattern", false, false) {
    const auto *self = (String *) _self;
    const auto *pattern = (String *) args[0];

    return (ArObject *) IntNew(support::Count(STR_BUF(self), STR_LEN(self), STR_BUF(pattern),
                                              STR_LEN(pattern), -1));
}

ARGON_METHOD(str_endswith, endswith,
             "Returns true if the string ends with the specified value.\n"
             "\n"
             "- Parameter pattern: The value to check if the string ends with.\n"
             "- Returns: True if the string ends with the specified value, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- startswith\n",
             "s: pattern", false, false) {

    return BoolToArBool(StringEndswith((String *) _self, (String *) args[0]));
}

ARGON_METHOD(str_find, find,
             "Searches the string for a specified value and returns the position of where it was found.\n"
             "\n"
             "- Parameter pattern: The value to search for.\n"
             "- Returns: Index of the first position, -1 otherwise.\n"
             "\n"
             "# SEE\n"
             "- rfind\n",
             "s: pattern", false, false) {
    return (ArObject *) IntNew(StringFind((String *) _self, (String *) args[0]));
}

ARGON_METHOD(str_isdigit, isdigit,
             "Check if all characters in the string are digits.\n"
             "\n"
             "- Returns: True if all characters are digits, false otherwise.\n",
             nullptr, false, false) {
    auto *self = (const String *) _self;

    for (ArSize i = 0; i < STR_LEN(self); i++) {
        if (!std::isdigit(STR_BUF(self)[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_METHOD(str_lower, lower,
             "Return a copy of the string converted to lowercase.\n"
             "\n"
             "- Returns: New string with all characters converted to lowercase.\n",
             nullptr, false, false) {
    const auto *self = (String *) _self;
    unsigned char *buf;
    String *ret;

    if ((buf = (unsigned char *) argon::vm::memory::Alloc(STR_LEN(self) + 1)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < STR_LEN(self); i++)
        buf[i] = (unsigned char) tolower(self->buffer[i]);

    buf[STR_LEN(self)] = '\0';

    ret = StringNew(buf, STR_LEN(self), self->cp_length, self->kind);
    if (ret == nullptr) {
        argon::vm::memory::Free(buf);
        return nullptr;
    }

    return (ArObject *) ret;
}

ARGON_METHOD(str_ord, ord,
             "Return the unicode code point for a one-character string.\n"
             "\n"
             "- Returns: Unicode code point.\n",
             nullptr, false, false) {
    const auto *self = (String *) _self;

    if (self->cp_length == 0) {
        ErrorFormat(kTypeError[0], "%s expected a character, but string of length 0 found",
                    ARGON_RAW_STRING(((Function *) _func)->qname));

        return nullptr;
    }

    if (self->cp_length > 1) {
        ErrorFormat(kTypeError[0], "%s expected a character, but string of length %d found",
                    ARGON_RAW_STRING(((Function *) _func)->qname), self->cp_length);

        return nullptr;
    }

    return (ArObject *) IntNew(StringUTF8ToInt(STR_BUF(self)));
}

ARGON_METHOD(str_replace, replace,
             "Returns a string where a specified value is replaced with a specified value.\n"
             "\n"
             "- Parameters:\n"
             " - old: String to search for.\n"
             " - new: String to replace the old value with.\n"
             " - count: Number specifying how many occurrences of the old value you want to replace.\n"
             "          To replace all occurrence use -1.\n"
             "- Returns: String where a specified value is replaced.\n",
             "s: old, s: new, i: count", false, false) {
    return (ArObject *) StringReplace((String *) _self, (String *) args[0],
                                      (String *) args[1], ((Integer *) args[2])->sint);
}

ARGON_METHOD(str_rfind, rfind,
             "Searches the string for a specified value and returns the last position of where it was found.\n"
             "\n"
             "- Parameter pattern: The value to search for.\n"
             "- Returns: Index of the last position, -1 otherwise.\n"
             "\n"
             "# SEE\n"
             "- find\n",
             "s: pattern", false, false) {
    return (ArObject *) IntNew(StringRFind((String *) _self, (String *) args[0]));
}

ARGON_METHOD(str_join, join,
             "Joins the elements of an iterable to the end of the string.\n"
             "\n"
             "- Parameter iterable: Any iterable object where all the returned values are strings.\n"
             "- Returns: New string where all items in an iterable are joined into one string.\n",
             ": iterable", false, false) {
    StringBuilder builder;
    const auto *self = (String *) _self;
    ArObject *iter;
    Error *error;
    String *tmp;

    ArSize idx = 0;

    if ((iter = IteratorGet(args[0], false)) == nullptr)
        return nullptr;

    while ((tmp = (String *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_string_)) {
            Release(tmp);
            Release(iter);

            ErrorFormat(kTypeError[0], "sequence item %i: expected string not '%s'", idx, AR_TYPE_NAME(tmp));

            return nullptr;
        }

        if (idx > 0 && !builder.Write(self, STR_LEN(tmp))) {
            Release(tmp);
            Release(iter);

            error = builder.GetError();

            argon::vm::Panic((ArObject *) error);

            Release(error);

            return nullptr;
        }

        if (!builder.Write(tmp, 0)) {
            error = builder.GetError();

            argon::vm::Panic((ArObject *) error);

            Release(error);

            return nullptr;
        }

        Release(tmp);
        idx++;
    }

    Release(iter);

    if ((tmp = builder.BuildString()) == nullptr) {
        error = builder.GetError();

        argon::vm::Panic((ArObject *) error);

        Release(error);
    }

    return (ArObject *) tmp;
}

ARGON_METHOD(str_split, split,
             "Splits the string at the specified separator and returns a list.\n"
             "\n"
             "- Parameters:\n"
             " - pattern: Specifies the separator to use when splitting the string.\n"
             " - maxsplit: Specifies how many splits to do.\n"
             "- Returns: New list of string.\n",
             "sn: pattern, i: maxsplit", false, false) {
    const unsigned char *pattern = nullptr;
    ArSize plen = 0;

    if (!IsNull(args[0])) {
        pattern = STR_BUF((String *) args[0]);
        plen = STR_LEN((String *) args[0]);

        if (plen == 0) {
            ErrorFormat(kValueError[0], "empty separator");
            return nullptr;
        }
    }

    return support::Split(STR_BUF((String *) _self),
                          pattern,
                          (support::SplitChunkNewFn<String>) StringNew,
                          STR_LEN((String *) _self),
                          plen,
                          ((Integer *) args[1])->sint);
}

ARGON_METHOD(str_startswith, startswith,
             "Returns true if the string starts with the specified value.\n"
             "\n"
             "- Parameter pattern: The value to check if the string starts with.\n"
             "- Returns: True if the string starts with the specified value, false otherwise.\n"
             "\n"
             "# SEE\n"
             "- endswith\n",
             "s: pattern", false, false) {
    const auto *self = (String *) _self;
    const auto *pattern = (String *) args[0];
    ArSSize n;

    n = (ArSSize) (STR_LEN(self) - STR_LEN(pattern));

    return BoolToArBool(n >= 0 &&
                        argon::vm::memory::MemoryCompare(STR_BUF(self), STR_BUF(pattern), STR_LEN(pattern)) == 0);
}

ARGON_METHOD(str_trim, trim,
             "Returns a new string stripped of whitespace from both ends.\n"
             "\n"
             "- Returns: New string without whitespace.\n",
             nullptr, false, false) {
    const auto *self = (String *) _self;
    ArSize start = 0;
    ArSize end = STR_LEN(self);

    while (STR_BUF(self)[start] == 0x09 || STR_BUF(self)[start] == 0x20)
        start++;

    while (STR_BUF(self)[end - 1] == 0x09 || STR_BUF(self)[end - 1] == 0x20)
        end--;

    return (ArObject *) StringNew((const char *) STR_BUF(self) + start, end - start);
}

ARGON_FUNCTION(str_unescape, unescape,
               "Unescapes any literals found in the string.\n"
               "\n"
               "- Parameter str: The string to unescape.\n"
               "- Returns: New unescaped string.\n",
               ": str", false, false) {
    StringBuilder builder;
    ArBuffer buffer = {};
    ArObject *ret;

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    builder.ParseEscaped(buffer.buffer, buffer.length);

    BufferRelease(&buffer);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ARGON_METHOD(str_upper, upper,
             "Return a copy of the string converted to uppercase.\n"
             "\n"
             "- Returns: New string with all characters converted to uppercase.\n",
             nullptr, false, false) {
    const auto *self = (String *) _self;
    unsigned char *buf;
    String *ret;

    if ((buf = (unsigned char *) argon::vm::memory::Alloc(STR_LEN(self) + 1)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < STR_LEN(self); i++)
        buf[i] = (unsigned char) toupper(self->buffer[i]);

    buf[STR_LEN(self)] = '\0';

    ret = StringNew(buf, STR_LEN(self), self->cp_length, self->kind);
    if (ret == nullptr) {
        argon::vm::memory::Free(buf);
        return nullptr;
    }

    return (ArObject *) ret;
}

const FunctionDef string_methods[] = {
        str_string,

        str_capitalize,
        str_chr,
        str_count,
        str_endswith,
        str_find,
        str_isdigit,
        str_lower,
        str_ord,
        str_replace,
        str_rfind,
        str_join,
        str_split,
        str_startswith,
        str_trim,
        str_unescape,
        str_upper,
        ARGON_METHOD_SENTINEL
};

ArObject *string_kind_get(const String *self) {
    if (self->kind == StringKind::ASCII)
        return (ArObject *) AtomNew("ascii");

    return (ArObject *) AtomNew("utf8");
}

const MemberDef string_members[] = {
        ARGON_MEMBER("intern", MemberType::BOOL, offsetof(String, intern), true),
        ARGON_MEMBER_GETSET("kind", (MemberGetFn) string_kind_get, nullptr),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots string_objslot = {
        string_methods,
        string_members,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *string_get_item(const String *self, ArObject *index) {
    ArSSize idx;

    if (self->kind != StringKind::ASCII) {
        ErrorFormat(kUnicodeError[0], kUnicodeError[2]);
        return nullptr;
    }

    if (AR_TYPEOF(index, type_int_)) {
        idx = ((Integer *) index)->sint;
        if (idx < 0)
            idx = (ArSSize) (STR_LEN(self) + idx);
    } else {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_NAME(index));
        return nullptr;
    }

    if (idx >= self->length)
        ErrorFormat(kOverflowError[0], kOverflowError[1], type_string_->name, self->length, idx);

    return (ArObject *) StringIntern((const char *) STR_BUF(self) + idx, 1);
}

ArObject *string_get_slice(const String *self, ArObject *bounds) {
    auto *b = (Bounds *) bounds;
    String *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    if (self->kind != StringKind::ASCII) {
        ErrorFormat(kUnicodeError[0], kUnicodeError[3]);
        return nullptr;
    }

    slice_len = BoundsIndex(b, STR_LEN(self), &start, &stop, &step);

    if ((ret = StringInit(slice_len, true)) == nullptr)
        return nullptr;

    ret->cp_length = slice_len;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step)
            ret->buffer[i++] = self->buffer[start];
    } else {
        for (ArSize i = 0; stop < start; start += step)
            ret->buffer[i++] = self->buffer[start];
    }

    return (ArObject *) ret;
}

ArObject *string_in(const String *self, const ArObject *value) {
    const auto *pattern = (const String *) value;

    if (!AR_TYPEOF(value, type_string_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_string_->name, AR_TYPE_NAME(value));
        return nullptr;
    }

    if (self == pattern)
        return BoolToArBool(true);

    return BoolToArBool(StringFind(self, pattern) >= 0);
}

ArSize string_length(const String *self) {
    return STR_LEN(self);
}

const SubscriptSlots string_subscript = {
        (ArSize_UnaryOp) string_length,
        (BinaryOp) string_get_item,
        nullptr,
        (BinaryOp) string_get_slice,
        nullptr,
        (BinaryOp) string_in
};

ArObject *string_add(String *left, String *right) {
    if (AR_TYPEOF(left, type_string_) && AR_SAME_TYPE(left, right))
        return (ArObject *) StringConcat(left, right);

    return nullptr;
}

ArObject *string_mod(const String *left, ArObject *fmt) {
    return (ArObject *) StringFormat((const char *) STR_BUF(left), fmt);
}

ArObject *string_mul(const String *left, const ArObject *right) {
    const auto *l = left;
    String *ret = nullptr;
    UIntegerUnderlying times;

    // int * str -> str * int
    if (!AR_TYPEOF(left, type_string_)) {
        l = (const String *) right;
        right = (const ArObject *) left;
    }

    if (!AR_TYPEOF(right, type_int_) && !AR_TYPEOF(right, type_uint_))
        return nullptr;

    times = ((const Integer *) right)->uint;

    if (AR_TYPEOF(right, type_int_)) {
        if (((const Integer *) right)->sint < 0) {
            ErrorFormat(kValueError[0], "string cannot be multiplied by a negative value");
            return nullptr;
        }

        times = ((const Integer *) right)->sint;
    }

    if ((ret = StringInit(l->length * times, true)) != nullptr) {
        ret->cp_length = l->cp_length * times;

        while (times--)
            argon::vm::memory::MemoryCopy(ret->buffer + l->length * times, l->buffer, l->length);

        ret->kind = l->kind;
    }

    return (ArObject *) ret;
}

const OpSlots string_ops = {
        (BinaryOp) string_add,
        nullptr,
        (BinaryOp) string_mul,
        nullptr,
        nullptr,
        (BinaryOp) string_mod,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) string_add,
        nullptr,
        nullptr,
        nullptr
};

ArObject *string_compare(const String *self, const ArObject *other, CompareMode mode) {
    const auto *o = (const String *) other;
    int left = 0;
    int right = 0;
    int res;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    if (mode == CompareMode::EQ && self->kind != o->kind)
        return BoolToArBool(false);

    res = StringCompare(self, o);
    if (res < 0)
        left = -1;
    else if (res > 0)
        right = -1;

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArObject *string_iter(String *self, bool reverse) {
    auto *si = MakeObject<StringIterator>(type_string_iterator_);

    if (si != nullptr) {
        new(&si->lock)std::mutex;

        si->iterable = IncRef(self);
        si->index = 0;
        si->reverse = reverse;
    }

    return (ArObject *) si;
}

ArObject *string_str(String *self) {
    return (ArObject *) IncRef(self);
}

ArObject *string_repr(const String *self) {
    StringBuilder builder;

    builder.Write((const unsigned char *) "\"", 1, STR_LEN(self) + 1);
    builder.WriteEscaped(STR_BUF(self), STR_LEN(self), 1, true);
    builder.Write((const unsigned char *) "\"", 1, 0);

    auto *ret = (ArObject *) builder.BuildString();
    if (ret == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

ArSize string_hash(String *self) {
    if (self->hash == 0)
        self->hash = HashBytes(STR_BUF(self), STR_LEN(self));

    return self->hash;
}

bool string_dtor(String *self) {
    argon::vm::memory::Free(STR_BUF(self));

    return true;
}

bool string_istrue(const String *self) {
    return STR_LEN(self) > 0;
}

TypeInfo StringType = {
        AROBJ_HEAD_INIT_TYPE,
        "String",
        nullptr,
        nullptr,
        sizeof(String),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) string_dtor,
        nullptr,
        (ArSize_UnaryOp) string_hash,
        (Bool_UnaryOp) string_istrue,
        (CompareOp) string_compare,
        (UnaryConstOp) string_repr,
        (UnaryOp) string_str,
        (UnaryBoolOp) string_iter,
        nullptr,
        &string_buffer,
        nullptr,
        &string_objslot,
        &string_subscript,
        &string_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_string_ = &StringType;

ArObject *argon::vm::datatype::StringSplit(const String *string, const unsigned char *pattern,
                                           ArSize plen, ArSSize maxsplit) {
    return support::Split(STR_BUF(string), pattern, (support::SplitChunkNewFn<String>) StringNew,
                          STR_LEN(string), plen, maxsplit);
}

ArSize argon::vm::datatype::StringSubstrLen(const String *string, ArSize offset, ArSize graphemes) {
    const unsigned char *buf = STR_BUF(string) + offset;
    const unsigned char *end = STR_BUF(string) + string->length;

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

    return buf - (STR_BUF(string) + offset);
}

bool argon::vm::datatype::StringEndswith(const String *string, const String *pattern) {
    auto n = (ArSSize) (STR_LEN(string) - STR_LEN(pattern));

    return n >= 0 && memory::MemoryCompare(STR_BUF(string) + n, STR_BUF(pattern), STR_LEN(pattern)) == 0;
}

bool argon::vm::datatype::StringEndswith(const String *string, const char *pattern) {
    auto plen = strlen(pattern);

    auto n = (ArSSize) (STR_LEN(string) - plen);

    return n >= 0 && memory::MemoryCompare(STR_BUF(string) + n, pattern, plen) == 0;
}

int argon::vm::datatype::StringCompare(const String *left, const String *right) {
    ArSize idx1 = 0;
    ArSize idx2 = 0;

    unsigned char c1;
    unsigned char c2;

    do {
        c1 = ARGON_RAW_STRING(left)[idx1++];
        c2 = ARGON_RAW_STRING(right)[idx2++];

        // Take care of '\0', String->len not include '\0'
    } while (c1 == c2 && idx1 < ARGON_RAW_STRING_LENGTH(left) + 1);

    return c1 - c2;
}

String *argon::vm::datatype::StringConcat(String *left, String *right) {
    String *ret;

    if (left == nullptr && right == nullptr)
        return StringIntern("");
    else if (left == nullptr)
        return IncRef(right);
    else if (right == nullptr)
        return IncRef(left);

    if ((ret = StringInit(STR_LEN(left) + STR_LEN(right), true)) != nullptr) {
        memory::MemoryCopy(ret->buffer, STR_BUF(left), STR_LEN(left));
        memory::MemoryCopy(ret->buffer + STR_LEN(left), STR_BUF(right), STR_LEN(right));

        ret->kind = left->kind;

        if (right->kind > left->kind)
            ret->kind = right->kind;

        ret->cp_length = left->cp_length + right->cp_length;
    }

    return ret;
}

String *argon::vm::datatype::StringConcat(String *left, const char *string, ArSize length) {
    auto *astr = StringNew(string, length);
    if (astr == nullptr)
        return nullptr;

    auto *concat = StringConcat(left, astr);

    Release(astr);

    return concat;
}

String *argon::vm::datatype::StringFormat(const char *format, ...) {
    va_list args;
    String *str;

    va_start(args, format);
    str = StringFormat(format, args);
    va_end(args);

    return str;
}

String *argon::vm::datatype::StringFormat(const char *format, va_list args) {
    va_list vargs2;
    String *str;
    int sz;

    va_copy(vargs2, args);
    sz = vsnprintf(nullptr, 0, format, vargs2) + 1; // +1 is for '\0'
    va_end(vargs2);

    if ((str = StringInit(sz - 1, true)) == nullptr)
        return nullptr;

    str->cp_length = sz - 1;

    va_copy(vargs2, args);
    vsnprintf((char *) STR_BUF(str), sz, format, vargs2);
    va_end(vargs2);

    return str;
}

String *argon::vm::datatype::StringFormat(const char *format, ArObject *args) {
    StringFormatter fmt(format, args, false);
    String *ret;

    unsigned char *buffer;
    ArSize out_length;
    ArSize out_cap;

    if ((buffer = fmt.Format(&out_length, &out_cap)) == nullptr) {
        auto *err = fmt.GetError();

        argon::vm::Panic((ArObject *) err);

        Release(err);

        return nullptr;
    }

    if ((ret = StringNew(buffer, out_length, out_length, StringKind::ASCII)) == nullptr)
        return nullptr;

    fmt.ReleaseOwnership();

    if (!StringInitKind(ret))
        Release((ArObject **) &ret);

    return ret;
}

String *argon::vm::datatype::StringIntern(const char *string, ArSize length) {
    String *ret;

    // Initialize intern
    if (intern == nullptr) {
        intern = DictNew();
        if (intern == nullptr)
            return nullptr;

        // Insert empty string
        if ((ret = StringInit(0, true)) == nullptr)
            return nullptr;

        if (!DictInsert(intern, (ArObject *) ret, (ArObject *) ret)) {
            Release(ret);
            return nullptr;
        }

        ret->intern = true;

        empty_string = ret;

        if (string == nullptr || length == 0)
            return ret;

        Release(ret);
    }

    if (string == nullptr || length == 0)
        return IncRef(empty_string);

    if ((ret = (String *) DictLookup(intern, string, length)) == nullptr) {
        if ((ret = StringNew(string, length)) == nullptr)
            return nullptr;

        if (!DictInsert(intern, (ArObject *) ret, (ArObject *) ret)) {
            Release(ret);
            return nullptr;
        }

        ret->intern = true;
    }

    return ret;
}

String *argon::vm::datatype::StringNew(const char *string, ArSize length) {
    StringBuilder builder;
    Error *error;
    String *str;

    builder.Write((const unsigned char *) string, length, 0);

    if ((str = builder.BuildString()) == nullptr) {
        error = builder.GetError();

        argon::vm::Panic((ArObject *) error);

        Release(error);

        return nullptr;
    }

    return str;
}

String *argon::vm::datatype::StringNew(unsigned char *buffer, ArSize length, ArSize cp_length, StringKind kind) {
    assert(buffer[length] == '\0');

    auto *str = StringInit(length, false);

    if (str != nullptr) {
        str->buffer = buffer;
        str->cp_length = cp_length;
        str->kind = kind;
    }

    return str;
}

String *argon::vm::datatype::StringNewHoldBuffer(unsigned char *buffer, ArSize length) {
    assert(buffer[length] == '\0');

    auto *str = StringInit(length, false);

    if (str != nullptr) {
        str->buffer = buffer;

        if (!StringInitKind(str)) {
            str->buffer = nullptr;

            Release(str);

            return nullptr;
        }
    }

    return str;
}

String *argon::vm::datatype::StringReplace(String *string, const String *old, const String *nval, ArSSize n) {
    StringBuilder builder;
    Error *error;
    ArSize idx = 0;
    ArSize newsz;

    String *ret;

    if (Equal((const ArObject *) string, (const ArObject *) old) || n == 0)
        return IncRef(string);

    // Compute replacements
    n = support::Count(STR_BUF(string), STR_LEN(string), STR_BUF(old), STR_LEN(old), n);

    newsz = (STR_LEN(string) + n * (STR_LEN(nval) - STR_LEN(old)));

    // Allocate string
    if (!builder.BufferResize(newsz)) {
        error = builder.GetError();

        argon::vm::Panic((ArObject *) error);

        Release(error);

        return nullptr;
    }

    long match;
    while ((match = support::Find(STR_BUF(string) + idx, STR_LEN(string) - idx, STR_BUF(old), STR_LEN(old))) > -1) {
        builder.Write(STR_BUF(string) + idx, match, 0);

        idx += match + STR_LEN(old);

        builder.Write(nval->buffer, STR_LEN(nval), 0);

        if (n > -1) {
            n--;
            if (n == 0)
                break;
        }
    }

    builder.Write(STR_BUF(string) + idx, STR_LEN(string) - idx, 0);

    if ((ret = builder.BuildString()) == nullptr) {
        error = builder.GetError();

        argon::vm::Panic((ArObject *) error);

        Release(error);

        return nullptr;
    }

    return ret;
}

String *argon::vm::datatype::StringSubs(const String *string, ArSize start, ArSize end) {
    StringBuilder builder;
    String *ret;
    ArSize len;

    if (start >= STR_LEN(string))
        return nullptr;

    if (end == 0 || end > STR_LEN(string))
        end = STR_LEN(string);

    if (start >= end)
        return nullptr;

    len = end - start;

    if (string->kind != StringKind::ASCII)
        len = StringSubstrLen(string, start, end - start);

    builder.Write(string->buffer + start, len, 0);

    if ((ret = builder.BuildString()) == nullptr) {
        auto *error = builder.GetError();

        argon::vm::Panic((ArObject *) error);

        Release(error);

        return nullptr;
    }

    return ret;
}

// STRING ITERATOR

ArObject *stringiterator_iter_next(StringIterator *self) {
    const unsigned char *buf;
    String *ret = nullptr;
    ArSize len = 1;

    std::unique_lock iter_lock(self->lock);

    buf = STR_BUF(self->iterable) + self->index;

    if (!self->reverse) {
        if (self->index < STR_LEN(self->iterable)) {
            len = StringSubstrLen(self->iterable, self->index, 1);

            if ((ret = StringIntern((const char *) buf, len)) == nullptr)
                return nullptr;

            self->index += len;
        }

        return (ArObject *) ret;
    }

    if (STR_LEN(self->iterable) - self->index == 0)
        return nullptr;

    buf--;

    while (buf > STR_BUF(self->iterable) && (*buf >> 6 == 0x2)) {
        buf--;
        len++;
    }

    if ((ret = StringIntern((const char *) buf, len)) != nullptr)
        return nullptr;

    self->index -= len;

    return (ArObject *) ret;
}

TypeInfo StringIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "StringIterator",
        nullptr,
        nullptr,
        sizeof(StringIterator),
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
        (UnaryOp) stringiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_string_iterator_ = &StringIteratorType;