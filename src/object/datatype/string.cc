// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>
#include <cmath>

#include <memory/memory.h>
#include <vm/runtime.h>

#include "error.h"
#include "hash_magic.h"
#include "bool.h"
#include "bounds.h"
#include "decimal.h"
#include "integer.h"
#include "iterator.h"
#include "map.h"
#include "string.h"

using namespace argon::memory;
using namespace argon::object;

static Map *intern;

ArSize StringSubStrLen(String *str, ArSize offset, ArSize graphemes) {
    unsigned char *buf = str->buffer + offset;
    unsigned char *end = str->buffer + str->len;

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

ITERATOR_NEW_DEFAULT(str_iterator, (BoolUnaryOp) str_iter_has_next, (UnaryOp) str_iter_next, (UnaryOp) str_iter_peek);

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
    return BufferSimpleFill(self, buffer, flags, self->buffer, self->len, false);
}

BufferSlots string_buffer = {
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

OpSlots string_ops{
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

SequenceSlots string_sequence = {
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
    StringBuilder builder = {};
    auto *str = (String *) self;
    String *tmp;
    ArObject *iter;

    ArSize idx = 0;

    if ((iter = IteratorGet(argv[0])) == nullptr)
        return nullptr;

    while ((tmp = (String *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_string_)) {
            ErrorFormat(type_type_error_, "sequence item %i: expected string not '%s'", idx, AR_TYPE_NAME(tmp));
            goto error;
        }

        if (idx > 0 && StringBuilderWrite(&builder, str->buffer, str->len, tmp->len) < 0)
            goto error;

        if (StringBuilderWrite(&builder, tmp->buffer, tmp->len) < 0)
            goto error;

        Release(tmp);
        idx++;
    }

    Release(iter);
    return StringBuilderFinish(&builder);

    error:
    Release(tmp);
    Release(iter);
    StringBuilderClean(&builder);
    return nullptr;
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

const NativeFunc str_methods[] = {
        str_new_,
        str_count_,
        str_endswith_,
        str_find_,
        str_replace_,
        str_rfind_,
        str_join_,
        str_split_,
        str_startswith_,
        str_trim_,
        str_chr_,
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

String *string_str(String *self) {
    /*
    StringBuilder sb = {};
    ArSize len = self->len + 1;

    if (StringBuilderWrite(&sb, (unsigned char *) "\"", 1, len) < 0)
        goto error;

    if (StringBuilderWrite(&sb, self->buffer, self->len) < 0)
        goto error;

    if (StringBuilderWrite(&sb, (unsigned char *) "\"", 1) < 0)
        goto error;

    return StringBuilderFinish(&sb);

    error:
    StringBuilderClean(&sb);
    return nullptr;
     */

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

    va_start (args, string);
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

// String Builder

bool argon::object::StringBuilderResize(StringBuilder *sb, ArSize len) {
    unsigned char *tmp;

    if (len == 0 || sb->w_idx + len < sb->str.len)
        return true;

    if (sb->str.buffer == nullptr)
        len += 1;

    if ((tmp = (unsigned char *) argon::memory::Realloc(sb->str.buffer, sb->str.len + len)) != nullptr) {
        sb->str.buffer = tmp;
        sb->str.len += len;
        return true;
    }

    argon::vm::Panic(error_out_of_memory);
    return false;
}

bool
argon::object::StringBuilderResizeAscii(StringBuilder *sb, const unsigned char *buffer, ArSize len, int overalloc) {
    ArSize str_len = overalloc;

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
                if (buffer[i] < ' ' || buffer[i] > 0x7F) {
                    str_len += 4;
                    break;
                }
                str_len++;
        }
    }

    return StringBuilderResize(sb, str_len);
}

int argon::object::StringBuilderRepeat(StringBuilder *sb, char chr, int times) {
    if (!StringBuilderResize(sb, times))
        return -1;

    for (int i = 0; i < times; i++)
        sb->str.buffer[sb->w_idx++] = chr;

    sb->str.cp_len += times;

    return times;
}

int argon::object::StringBuilderWrite(StringBuilder *sb, const unsigned char *buffer, ArSize len, int overalloc) {
    ArSize wbytes;

    if (!StringBuilderResize(sb, len + overalloc))
        return -1;

    wbytes = FillBuffer(&sb->str, sb->w_idx, buffer, len);
    sb->w_idx += wbytes;

    return (int) wbytes;
}

int argon::object::StringBuilderWriteAscii(StringBuilder *sb, const unsigned char *buffer, ArSize len) {
    static const unsigned char hex[] = "0123456789abcdef";
    unsigned char *start = sb->str.buffer + sb->w_idx;
    unsigned char *buf = sb->str.buffer + sb->w_idx;

    for (ArSize i = 0; i < len; i++) {
        if (buf - start > sb->str.len - sb->w_idx)
            return -1;

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
                if (buffer[i] < ' ' || buffer[i] > 0x7F) {
                    *buf++ = '\\';
                    *buf++ = 'x';
                    *buf++ = hex[(buffer[i] & 0xF0) >> 4];
                    *buf++ = hex[(buffer[i] & 0x0F)];
                    break;
                }
                *buf++ = buffer[i];
        }
    }

    sb->str.cp_len += buf - start;
    sb->w_idx += buf - start;
    return len;
}

int argon::object::StringBuilderWriteHex(StringBuilder *sb, const unsigned char *buffer, ArSize len) {
    static const unsigned char hex[] = "0123456789abcdef";
    unsigned char *buf;

    ArSize wlen = len * 4;

    if (!StringBuilderResize(sb, wlen))
        return -1;

    buf = sb->str.buffer;

    for (ArSize i = 0; i < len; i++) {
        *buf++ = '\\';
        *buf++ = 'x';
        *buf++ = hex[(buffer[i] & 0xF0) >> 4];
        *buf++ = hex[(buffer[i] & 0x0F)];
    }

    sb->w_idx += wlen;
    sb->str.cp_len += wlen;

    return (int) wlen;
}

String *argon::object::StringBuilderFinish(StringBuilder *sb) {
    String *str;

    if (sb->str.len == 0)
        return StringIntern((const char *) "");

    if ((str = StringInit(sb->str.len - 1, false)) != nullptr) {
        str->buffer = sb->str.buffer;
        str->buffer[str->len] = 0x00;
        str->kind = sb->str.kind;
        str->cp_len = sb->str.cp_len;
        return str;
    }

    // Release StringBuilder buffer
    StringBuilderClean(sb);
    return nullptr;
}

void argon::object::StringBuilderClean(StringBuilder *sb) {
    if (sb->str.buffer != nullptr)
        Free(sb->str.buffer);
    MemoryZero(sb, sizeof(StringBuilder));
}

// String Formatter

ArObject *FmtGetNextArg(StringFormatter *fmt) {
    if (fmt->args->type == type_tuple_) {
        auto tp = ((Tuple *) fmt->args);

        fmt->args_len = tp->len;

        if (fmt->args_idx < tp->len)
            return tp->objects[fmt->args_idx++];

        goto not_enough;
    }

    fmt->args_len = 1;
    if (fmt->args_idx++ == 0)
        return fmt->args;

    not_enough:
    return ErrorFormat(type_type_error_, "not enough arguments for format string");
}

int FmtGetNextSpecifier(StringFormatter *fmt) {
    unsigned char *buf = fmt->fmt.buf + fmt->fmt.idx;
    ArSize idx = 0;

    while ((fmt->fmt.idx + idx) < fmt->fmt.len) {
        if (buf[idx++] == '%') {

            if ((fmt->fmt.idx + idx) == fmt->fmt.len) {
                ErrorFormat(type_value_error_, "incomplete format specifier");
                return -1;
            }

            if (buf[idx] != '%') {
                fmt->nspec++;
                fmt->fmt.idx++;
                idx--;
                break;
            }
            idx++;
        }
    }

    if (StringBuilderWrite(&fmt->builder, buf, idx) < 0)
        return -1;

    fmt->fmt.idx += idx;

    return fmt->fmt.idx != fmt->fmt.len;
}

bool FmtParseOptionStar(StringFormatter *fmt, StringArg *arg, bool prec) {
    auto num = (Integer *) FmtGetNextArg(fmt);
    int opt;

    if (num == nullptr)
        return false;

    if (num->type != type_integer_) {
        ErrorFormat(type_type_error_, "* wants integer not '%s'", num->type->name);
        return false;
    }

    opt = (int)num->integer;

    if (opt < 0) {
        if (!prec)
            arg->flags |= StringFormatFlags::LJUST;
        opt = -opt;
    }

    if (!prec)
        arg->width = opt;
    else
        arg->prec = opt;

    fmt->nspec++;
    return true;
}

void FmtParseOptions(StringFormatter *fmt, StringArg *arg) {
    // Flags
    fmt->fmt.idx--;
    while (fmt->fmt.buf[fmt->fmt.idx++] >= 0) {
        switch (fmt->fmt.buf[fmt->fmt.idx]) {
            case '-':
                arg->flags |= StringFormatFlags::LJUST;
                continue;
            case '+':
                arg->flags |= StringFormatFlags::SIGN;
                continue;
            case ' ':
                arg->flags |= StringFormatFlags::BLANK;
                continue;
            case '#':
                arg->flags |= StringFormatFlags::ALT;
                continue;
            case '0':
                arg->flags |= StringFormatFlags::ZERO;
                continue;
            default:
                break;
        }
        break;
    }

    if (ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::LJUST) &&
        ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::ZERO))
        arg->flags &= ~StringFormatFlags::ZERO;

    // Width
    arg->width = 0;
    if (fmt->fmt.buf[fmt->fmt.idx] == '*') {
        fmt->fmt.idx++;
        if (!FmtParseOptionStar(fmt, arg, false))
            return;
    } else {
        if (fmt->fmt.buf[fmt->fmt.idx] >= '0' && fmt->fmt.buf[fmt->fmt.idx] <= '9') {
            arg->width = fmt->fmt.buf[fmt->fmt.idx++] - '0';

            while (fmt->fmt.buf[fmt->fmt.idx] >= '0' && fmt->fmt.buf[fmt->fmt.idx] <= '9') {
                arg->width = arg->width * 10 + (fmt->fmt.buf[fmt->fmt.idx] - '0');
                fmt->fmt.idx++;
            }
        }
    }

    // Precision
    arg->prec = -1;
    if (fmt->fmt.buf[fmt->fmt.idx] == '.') {
        fmt->fmt.idx++;
        if (fmt->fmt.buf[fmt->fmt.idx] == '*') {
            fmt->fmt.idx++;
            if (!FmtParseOptionStar(fmt, arg, true))
                return;
        } else {
            arg->prec = 0;
            while (fmt->fmt.buf[fmt->fmt.idx] >= '0' && fmt->fmt.buf[fmt->fmt.idx] <= '9') {
                arg->prec = arg->prec * 10 + (fmt->fmt.buf[fmt->fmt.idx] - '0');
                fmt->fmt.idx++;
            }
        }
    }
}

int FmtWrite(StringFormatter *fmt, StringArg *arg, const unsigned char *buf, ArSize len) {
    int width = 0;

    if (arg->width > len)
        width = (int) (arg->width - len);

    if (!StringBuilderResize(&fmt->builder, len + width))
        return -1;

    if (ENUMBITMASK_ISFALSE(arg->flags, StringFormatFlags::LJUST))
        width -= StringBuilderRepeat(&fmt->builder, ' ', width);

    StringBuilderWrite(&fmt->builder, buf, len);

    return width;
}

int
FmtNumberFormat(unsigned char *buf, int idx, int base, int width, bool upper, bool neg, StringFormatFlags flags) {
    unsigned char *end;
    unsigned char tmp;

    if (ENUMBITMASK_ISTRUE(flags, StringFormatFlags::ZERO)) {
        width = width > idx ? width - idx : 0;
        while (width--)
            buf[idx++] = '0';
    }

    if (ENUMBITMASK_ISTRUE(flags, StringFormatFlags::ALT)) {
        if (base == 2)
            buf[idx++] = upper ? 'B' : 'b';
        else if (base == 8)
            buf[idx++] = 'o';
        else if (base == 16)
            buf[idx++] = upper ? 'X' : 'x';

        buf[idx++] = '0';
    }

    if (!neg) {
        if (ENUMBITMASK_ISTRUE(flags, StringFormatFlags::SIGN))
            buf[idx++] = '+';
    } else
        buf[idx++] = '-';

    if (ENUMBITMASK_ISTRUE(flags, StringFormatFlags::BLANK))
        buf[idx++] = ' ';

    if (ENUMBITMASK_ISFALSE(flags, StringFormatFlags::LJUST)) {
        width = width > idx ? width - idx : 0;
        while (width--)
            buf[idx++] = ' ';
    }

    // invert
    end = buf + idx;
    while (buf < end) {
        tmp = *buf;
        *buf++ = *(end - 1);
        *--end = tmp;
    }

    return idx;
}

int
FmtWriteNumber(unsigned char *buf, long num, int base, int prec, int width, bool upper, StringFormatFlags flags) {
    static unsigned char l_case[] = "0123456789abcdef";
    static unsigned char u_case[] = "0123456789ABCDEF";
    unsigned char *p_case = upper ? u_case : l_case;
    ArSize idx = 0;
    bool neg = false;

    if (num < 0) {
        num = -num;
        neg = true;
    } else if (num == 0)
        buf[idx++] = '0';

    while (num) {
        buf[idx++] = p_case[num % base];
        num /= base;
    }

    if (prec > idx) {
        prec -= idx;
        while (prec--)
            buf[idx++] = '0';
    }

    return FmtNumberFormat(buf, idx, base, width, upper, neg, flags);
}

#define SB_GET_BUFFER(sb)   ((sb).str.buffer + (sb).w_idx)

int FmtDecimal(StringFormatter *fmt, StringArg *arg, char specifier) {
#define FMT_PRECISION_DEF   6
    ArObject *obj;
    DecimalUnderlying num;

    unsigned long intpart;
    unsigned long frac;
    long exp;

    int bufsz;
    int diff;
    int prec;

    bool upper = false;
    bool sn = false;

    if ((obj = FmtGetNextArg(fmt)) == nullptr)
        return -1;

    if (obj->type == type_decimal_)
        num = ((Decimal *) obj)->decimal;
    else if (obj->type == type_integer_)
        num = ((Integer *) obj)->integer;
    else {
        ErrorFormat(type_type_error_, "%%%c requires real number not '%s'", fmt->fmt.buf[fmt->fmt.idx],
                    obj->type->name);
        return -1;
    }

    if (std::isnan(num)) // check NaN equals to num != num
        return FmtWrite(fmt, arg, (unsigned char *) "nan", 3);
    else if (num > DBL_MAX)
        return FmtWrite(fmt, arg, (unsigned char *) "+inf", 4);
    else if (num < -DBL_MAX)
        return FmtWrite(fmt, arg, (unsigned char *) "-inf", 4);

    prec = arg->prec > -1 ? arg->prec : FMT_PRECISION_DEF;

    switch (specifier) {
        case 'E':
            upper = true;
        case 'e':
            intpart = DecimalFrexp10(num, &frac, &exp, prec);
            sn = true;
            break;
        case 'G':
            upper = true;
        case 'g':
            intpart = DecimalFrexp10(num, &frac, &exp, prec);
            sn = true;
            if (num >= 1e-4 && num < 1e6) {
                if (prec > exp)
                    intpart = DecimalModf(num, &frac, prec - exp - 1);
                else
                    intpart = DecimalModf(num, &frac, 0);
                sn = false;
            }
            break;
        case 'F':
        case 'f':
            intpart = DecimalModf(num, &frac, prec);
            break;
        default:
            assert(false);
    }

    bufsz = IntegerCountDigits(intpart, 10);

    if (num < 0 || ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::SIGN))
        bufsz++;

    if (ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::BLANK))
        bufsz++;

    if (frac > 0) {
        bufsz += IntegerCountDigits(frac, 10);
        bufsz++; // '.'
    }

    if (sn) {
        auto len = IntegerCountDigits(exp, 10);
        if (len <= 1)
            len += 2;
        bufsz += len + 2; // [+|-][e|E]
    }

    bufsz += arg->width > bufsz ? arg->width - bufsz : 0;

    if (!StringBuilderResize(&fmt->builder, bufsz))
        return -1;

    diff = FmtWriteNumber(SB_GET_BUFFER(fmt->builder), (long) intpart, 10, 0, arg->width, upper, arg->flags);

    if (frac > 0) {
        fmt->builder.str.buffer[diff++ + fmt->builder.w_idx] = '.';
        diff += FmtWriteNumber(SB_GET_BUFFER(fmt->builder) + diff, (long) frac, 10, 0, 0, false,
                               (StringFormatFlags) 0);
    }

    if (sn) {
        fmt->builder.str.buffer[diff++ + fmt->builder.w_idx] = upper ? 'E' : 'e';
        diff += FmtWriteNumber(SB_GET_BUFFER(fmt->builder) + diff, (long) exp, 10, 2, 0, false,
                               StringFormatFlags::SIGN);
    }

    fmt->builder.w_idx += diff;
    fmt->builder.str.cp_len += diff;

    return bufsz - diff;
#undef FMT_PRECISION_DEF
}

int FmtInteger(StringFormatter *fmt, StringArg *arg, int base, bool upper) {
    ArObject *obj;

    IntegerUnderlying num;
    int bufsz;
    int diff;

    if ((obj = FmtGetNextArg(fmt)) == nullptr)
        return -1;

    if (obj->type != type_integer_) {
        ErrorFormat(type_type_error_, "%%%c requires integer not '%s'", fmt->fmt.buf[fmt->fmt.idx], obj->type->name);
        return -1;
    }

    num = ((Integer *) obj)->integer;
    bufsz = IntegerCountDigits(num, base);

    if (arg->prec > bufsz)
        bufsz = arg->prec;

    if (num < 0 || ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::SIGN))
        bufsz++;

    if (ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::ALT))
        bufsz += 2;

    if (ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::BLANK))
        bufsz++;

    bufsz += arg->width > bufsz ? arg->width - bufsz : 0;

    if (!StringBuilderResize(&fmt->builder, bufsz))
        return -1;

    diff = FmtWriteNumber(SB_GET_BUFFER(fmt->builder), num, base, arg->prec > -1 ? arg->prec : 0, arg->width, upper,
                          arg->flags);

    fmt->builder.w_idx += diff;
    fmt->builder.str.cp_len += diff;

    return bufsz - diff;
}

#undef SB_GET_BUFFER

int FmtChar(StringFormatter *fmt, StringArg *arg) {
    unsigned char sequence[] = {0, 0, 0, 0};
    ArObject *obj;
    int len;

    if ((obj = FmtGetNextArg(fmt)) == nullptr)
        return -1;

    if (obj->type == type_string_) {
        auto str = (String *) obj;
        if (str->cp_len > 1) {
            ErrorFormat(type_type_error_, "%%c requires a single char not string");
            return -1;
        }
        return FmtWrite(fmt, arg, str->buffer, str->len);
    } else if (obj->type == type_integer_) {
        if ((len = StringIntToUTF8(((Integer *) obj)->integer, sequence)) == 0) {
            ErrorFormat(type_overflow_error_, "%%c arg not in range(0x110000)");
            return -1;
        }

        return FmtWrite(fmt, arg, sequence, len);
    }

    ErrorFormat(type_type_error_, "%%c requires integer or char not '%s'", AR_TYPE_NAME(obj));
    return -1;
}

int FmtString(StringFormatter *fmt, StringArg *arg) {
    ArObject *obj;
    String *str;
    ArSize len;
    int ok;

    if ((obj = FmtGetNextArg(fmt)) == nullptr)
        return -1;

    str = (String *) obj->type->str(obj);
    len = str->len;

    if (arg->prec > -1 && len > arg->prec) {
        len = arg->prec;
        if (str->kind != StringKind::ASCII)
            len = StringSubStrLen(str, 0, len);
    }

    ok = FmtWrite(fmt, arg, str->buffer, len);
    Release(str);

    return ok;
}

bool FmtFormatArg(StringFormatter *fmt, StringArg *arg) {
    auto op = fmt->fmt.buf[fmt->fmt.idx];
    int result = -1;

    switch (op) {
        case 's':
            result = FmtString(fmt, arg);
            break;
        case 'b':
            result = FmtInteger(fmt, arg, 2, false);
            break;
        case 'B':
            result = FmtInteger(fmt, arg, 2, true);
            break;
        case 'o':
            result = FmtInteger(fmt, arg, 8, false);
            break;
        case 'i':
        case 'd':
        case 'u':
            arg->flags &= ~StringFormatFlags::ALT;
            result = FmtInteger(fmt, arg, 10, false);
            break;
        case 'x':
            result = FmtInteger(fmt, arg, 16, false);
            break;
        case 'X':
            result = FmtInteger(fmt, arg, 16, true);
            break;
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            result = FmtDecimal(fmt, arg, op);
            break;
        case 'c':
            result = FmtChar(fmt, arg);
            break;
        default:
            ErrorFormat(type_value_error_, "unsupported format character '%c' (0x%x)",
                        (31 <= op && op <= 126 ? '?' : op), op);
    }

    fmt->fmt.idx++;

    if (result < 0)
        return false;

    if (ENUMBITMASK_ISTRUE(arg->flags, StringFormatFlags::LJUST))
        StringBuilderRepeat(&fmt->builder, ' ', result);

    return true;
}

String *FmtFormatArgs(StringFormatter *fmt) {
    StringArg arg = {};
    String *str;

    int ok;

    argon::memory::MemoryZero(&fmt->builder, sizeof(StringBuilder));

    while ((ok = FmtGetNextSpecifier(fmt)) > 0) {
        FmtParseOptions(fmt, &arg);

        if (!FmtFormatArg(fmt, &arg))
            goto error;
    }

    if (ok < 0)
        goto error;

    if (fmt->nspec < fmt->args_len) {
        ErrorFormat(type_type_error_, "not all arguments converted during string formatting");
        goto error;
    }

    return StringBuilderFinish(&fmt->builder);

    error:
    if (fmt->builder.str.buffer != nullptr)
        argon::memory::Free(fmt->builder.str.buffer);

    return nullptr;
}

// Common Operations

ArObject *argon::object::StringSplit(String *string, String *pattern, ArSSize maxsplit) {
    String *tmp;
    List *ret;

    ArSize idx = 0;
    ArSSize end;
    ArSSize counter = 0;

    if ((ret = ListNew()) == nullptr)
        return nullptr;

    if (maxsplit != 0) {
        while ((end = support::Find(string->buffer + idx, string->len - idx, pattern->buffer, pattern->len)) >= 0) {
            if ((tmp = StringNew((const char *) string->buffer + idx, end)) == nullptr)
                goto error;

            idx += end + pattern->len;

            if (!ListAppend(ret, tmp))
                goto error;

            Release(tmp);

            if (maxsplit > -1 && ++counter >= maxsplit)
                break;
        }
    }

    if (string->len - idx > 0) {
        if ((tmp = StringNew((const char *) string->buffer + idx, string->len - idx)) == nullptr)
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

String *argon::object::StringCFormat(const char *fmt, ArObject *args) {
    StringFormatter formatter = {};

    formatter.fmt.buf = (unsigned char *) fmt;
    formatter.fmt.len = strlen(fmt);

    formatter.args = args;

    return FmtFormatArgs(&formatter);
}

String *argon::object::StringFormat(String *fmt, ArObject *args) {
    StringFormatter formatter = {};

    formatter.fmt.buf = fmt->buffer;
    formatter.fmt.len = fmt->len;

    formatter.args = args;

    return FmtFormatArgs(&formatter);
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
