// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>

#include "integer.h"
#include "stringbuilder.h"
#include "tuple.h"

#include "stringformatter.h"

using namespace argon::vm::datatype;

ArObject *StringFormatter::NextArg() {
    if (AR_TYPEOF(this->fmt_.args, type_tuple_)) {
        const auto *tp = (Tuple *) this->fmt_.args;

        this->fmt_.args_length = tp->length;

        if (this->fmt_.args_index < this->fmt_.args_length)
            return tp->objects[this->fmt_.args_index++];
    } else {
        this->fmt_.args_length = 1;

        if (this->fmt_.args_index++ == 0)
            return this->fmt_.args;
    }

    this->error_ = (ArObject *) ErrorNew(kTypeError[0], "not enough argument for format string");

    return nullptr;
}

ArSSize StringFormatter::FormatBytes() {
    ArBuffer buffer{};
    ArObject *obj;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (!BufferGet(obj, &buffer, BufferFlags::READ)) {
        this->error_ = argon::vm::GetLastError();
        return -1;
    }

    auto wlen = this->Write(buffer.buffer, buffer.length, 0);

    BufferRelease(&buffer);

    return wlen;
}

ArSSize StringFormatter::FormatString() {
    const ArObject *obj;
    String *str;
    ArSize str_len;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if ((str = (String *) Str(obj)) == nullptr)
        return -1;

    str_len = str->length;

    if (this->fmt_.prec > -1 && str_len > this->fmt_.prec) {
        str_len = str->kind != StringKind::ASCII ?
                  StringSubstrLen(str, 0, str_len) :
                  this->fmt_.prec;
    }

    auto wr = this->Write(str->buffer, str_len, 0);

    Release(str);

    return wr;
}

ArSSize StringFormatter::Write(const unsigned char *buffer, ArSize length, int overalloc) {
    if (!this->BufferResize(length + overalloc))
        return -1;

    const auto *buf = this->output_.cursor;

    this->output_.cursor = (unsigned char *) memory::MemoryCopy(this->output_.cursor, buffer, length);

    return this->output_.cursor - buf;
}

ArSSize StringFormatter::WriteRepeat(char ch, int times) {
    if (!this->BufferResize(times))
        return -1;

    for (int i = 0; i < times; i++) {
        *this->output_.cursor = ch;
        this->output_.cursor++;
    }

    return times;
}

unsigned char *StringFormatter::Format(ArSize *out_len, ArSize *out_cap) {
    if (this->output_.buffer != nullptr) {
        *out_len = this->output_.cursor - this->output_.buffer;
        *out_cap = this->output_.end - this->output_.buffer;

        return this->output_.buffer;
    }

    *out_len = 0;
    *out_cap = 0;

    while (this->NextSpecifier()) {
        if (!this->ParseOption())
            return nullptr;

        if (!this->Format())
            return nullptr;
    }

    *out_len = this->output_.cursor - this->output_.buffer;
    *out_cap = this->output_.end - this->output_.buffer;

    *this->output_.cursor = '\0';

    return this->output_.buffer;
}

bool StringFormatter::BufferResize(ArSize length) {
    ArSize cap = this->output_.end - this->output_.buffer;
    ArSize len = this->output_.end - this->output_.cursor;
    unsigned char *tmp;

    if (cap > 0)
        cap--; // space for '\0'

    if (length == 0 || len + length < cap)
        return true;

    if (this->output_.buffer == nullptr)
        length++;

    if ((tmp = (unsigned char *) memory::Realloc(this->output_.buffer, cap + length)) == nullptr) {
        this->error_ = argon::vm::GetLastError();
        return false;
    }

    this->output_.buffer = tmp;
    this->output_.cursor = this->output_.buffer + (cap - len);
    this->output_.end = this->output_.buffer + (cap + length);

    return true;
}

bool StringFormatter::Format() {
    auto op = *this->fmt_.cursor;
    ArSSize result = -1;

    switch (op) {
        case 's':
            result = this->string_as_bytes_ ? this->FormatBytes() : this->FormatString();
            break;
        case 'b':
            result = this->FormatInteger(2, false, false);
            break;
        case 'B':
            result = this->FormatInteger(2, false, true);
            break;
        case 'o':
            result = this->FormatInteger(8, false, false);
            break;
        case 'i':
        case 'd':
            this->fmt_.flags &= ~FormatFlags::ALT;
            result = this->FormatInteger(10, false, false);
            break;
        case 'u':
            this->fmt_.flags &= ~FormatFlags::ALT;
            result = this->FormatInteger(10, true, false);
            break;
        case 'x':
            result = this->FormatInteger(16, true, false);
            break;
        case 'X':
            result = this->FormatInteger(16, true, true);
            break;
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            result = this->FormatDecimal(op);
            break;
        case 'c':
            result = this->FormatChar();
            break;
        default:
            this->error_ = (ArObject *) ErrorNewFormat(kValueError[0],
                                                       "unsupported format character '%c' (0x%x)",
                                                       (31 <= op && op <= 126 ? '?' : op), op);
            break;
    }

    this->fmt_.cursor++;

    if (result < 0)
        return false;

    if (ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::LJUST))
        return this->WriteRepeat(' ', (int) result);

    return true;
}

bool StringFormatter::NextSpecifier() {
    const unsigned char *base = this->fmt_.cursor;
    int index = 0;

    while ((base + index) < this->fmt_.end) {
        if (*(base + index++) == '%') {
            if ((base + index) >= this->fmt_.end) {
                this->error_ = (ArObject *) ErrorNew(kValueError[0], "incomplete format specifier");
                return -1;
            }

            if (*(base + index) != '%') {
                this->fmt_.nspec++;
                this->fmt_.cursor++;
                index--;
                break;
            }

            index++;
        }
    }

    if (this->Write(base, index, 0) < 0)
        return -1;

    this->fmt_.cursor += index;

    return this->fmt_.cursor != this->fmt_.end;
}

bool StringFormatter::ParseOption() {
    // Flags
    while (*this->fmt_.cursor > 0) {
        switch (*this->fmt_.cursor) {
            case '-':
                this->fmt_.flags |= FormatFlags::LJUST;
                break;
            case '+':
                this->fmt_.flags |= FormatFlags::SIGN;
                break;
            case ' ':
                this->fmt_.flags |= FormatFlags::BLANK;
                break;
            case '#':
                this->fmt_.flags |= FormatFlags::ALT;
                break;
            case '0':
                this->fmt_.flags |= FormatFlags::ZERO;
                break;
            default:
                goto NEXT;
        }

        this->fmt_.cursor++;
    }

    NEXT:
    if (ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::LJUST) &&
        ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::ZERO))
        this->fmt_.flags &= ~FormatFlags::ZERO;

    // Width
    this->fmt_.width = 0;
    if (*this->fmt_.cursor == '*') {
        this->fmt_.cursor++;

        if (!this->ParseStarOption(false))
            return false;
    } else {
        while (isdigit(*this->fmt_.cursor)) {
            this->fmt_.width = this->fmt_.width * 10 + ((*this->fmt_.cursor) - '0');
            this->fmt_.cursor++;
        }
    }

    // Precision
    this->fmt_.prec = -1;
    if (*this->fmt_.cursor == '.') {
        this->fmt_.cursor++;

        if (*this->fmt_.cursor == '*') {
            this->fmt_.cursor++;

            if (!this->ParseStarOption(true))
                return false;
        } else {
            this->fmt_.prec = 0;

            while (isdigit(*this->fmt_.cursor)) {
                this->fmt_.prec = this->fmt_.prec * 10 + ((*this->fmt_.cursor) - '0');
                this->fmt_.cursor++;
            }
        }
    }

    return true;
}

bool StringFormatter::ParseStarOption(bool prec) {
    const auto *num = (Integer *) this->NextArg();
    IntegerUnderlying opt;

    if (num == nullptr)
        return false;

    if (!IsIntType((const ArObject *) num)) {
        this->error_ = (ArObject *) ErrorNewFormat(kTypeError[0], "* wants integer not '%s'", AR_TYPE_NAME(num));
        return false;
    }

    opt = num->sint;
    if (opt < 0) {
        if (!prec)
            this->fmt_.flags |= FormatFlags::LJUST;

        opt = -opt;
    }

    if (!prec)
        this->fmt_.width = (int) opt;
    else
        this->fmt_.prec = (int) opt;

    this->fmt_.nspec++;

    return true;
}

int StringFormatter::FormatChar() {
    unsigned char sequence[4]{};
    const ArObject *obj;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (AR_TYPEOF(obj, type_string_)) {
        const auto *str = (const String *) obj;

        if (str->cp_length > 1) {
            this->error_ = (ArObject *) ErrorNew(kTypeError[0], "%%c requires a single char not string");
            return -1;
        }

        return (int) this->Write(str->buffer, str->length, 0);
    } else if (AR_TYPEOF(obj, type_int_) || AR_TYPEOF(obj, type_uint_)) {
        auto slen = StringIntToUTF8((unsigned int) ((const Integer *) obj)->uint, sequence);

        if (slen == 0) {
            this->error_ = (ArObject *) ErrorNew(kOverflowError[0], "%%c arg not in range(0x110000)");
            return -1;
        }

        return (int) this->Write(sequence, slen, 0);
    }

    this->error_ = (ArObject *) ErrorNewFormat(kTypeError[0], "%%c requires integer or char not '%s'",
                                               AR_TYPE_NAME(obj));
    return -1;
}

int StringFormatter::FormatDecimal(unsigned char op) {
    // TODO: format decimal
    assert(false);
}

int StringFormatter::FormatInteger(int base, bool unsign, bool upper) {
    const Integer *number;

    int prec = 0;
    int bufsz;
    int diff;

    if ((number = (Integer *) this->NextArg()) == nullptr)
        return -1;

    if (!AR_TYPEOF(number, type_int_) && !AR_TYPEOF(number, type_uint_)) {
        this->error_ = (ArObject *) ErrorNewFormat(kTypeError[0], "%%%c requires integer not '%s'",
                                                   *this->fmt_.cursor, AR_TYPE_NAME(number));

        return -1;
    }

    if (!unsign)
        bufsz = IntegerCountDigits(number->sint, base);
    else
        bufsz = IntegerCountDigits(number->uint, base);

    if (this->fmt_.prec > bufsz)
        bufsz = this->fmt_.prec;

    if ((!unsign && (number->sint < 0)) || ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::SIGN))
        bufsz++;

    if (ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::ALT))
        bufsz += 2;

    if (ENUMBITMASK_ISTRUE(this->fmt_.flags, FormatFlags::BLANK))
        bufsz++;

    bufsz += this->fmt_.width > bufsz ? this->fmt_.width - bufsz : 0;

    if (!this->BufferResize(bufsz))
        return -1;

    if (this->fmt_.prec > -1)
        prec = this->fmt_.prec;

    if (!unsign)
        diff = StringFormatter::WriteNumber(this->output_.cursor,
                                            number->sint,
                                            base,
                                            prec,
                                            this->fmt_.width,
                                            upper,
                                            this->fmt_.flags);
    else
        diff = StringFormatter::WriteNumber(this->output_.cursor,
                                            number->uint,
                                            base,
                                            prec,
                                            this->fmt_.width,
                                            upper,
                                            this->fmt_.flags);

    this->output_.cursor += diff;

    return bufsz - diff;
}

int StringFormatter::FormatNumber(unsigned char *buf, int index, int base, int width, bool upper,
                                  bool neg, FormatFlags flags) {
    unsigned char *end;
    unsigned char tmp;

    if (ENUMBITMASK_ISTRUE(flags, FormatFlags::ZERO)) {
        width = width > index ? width - index : 0;
        while (width--)
            buf[index++] = '0';
    }

    if (ENUMBITMASK_ISTRUE(flags, FormatFlags::ALT)) {
        if (base == 2)
            buf[index++] = upper ? 'B' : 'b';
        else if (base == 8)
            buf[index++] = 'o';
        else if (base == 16)
            buf[index++] = upper ? 'X' : 'x';

        buf[index++] = '0';
    }

    if (!neg) {
        if (ENUMBITMASK_ISTRUE(flags, FormatFlags::SIGN))
            buf[index++] = '+';
    } else
        buf[index++] = '-';

    if (ENUMBITMASK_ISTRUE(flags, FormatFlags::BLANK))
        buf[index++] = ' ';

    if (ENUMBITMASK_ISFALSE(flags, FormatFlags::LJUST)) {
        width = width > index ? width - index : 0;
        while (width--)
            buf[index++] = ' ';
    }

    // invert
    end = buf + index;
    while (buf < end) {
        tmp = *buf;
        *buf++ = *(end - 1);
        *--end = tmp;
    }

    return index;
}

StringFormatter::~StringFormatter() {
    vm::memory::Free(this->output_.buffer);
}

ArObject *StringFormatter::GetError() {
    return this->error_;
}

void StringFormatter::ReleaseOwnership() {
    this->output_.buffer = nullptr;
    this->output_.cursor = nullptr;
    this->output_.end = nullptr;
}