// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>

#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/string.h>
#include <object/datatype/tuple.h>

#include "formatter.h"

using namespace argon::object;
using namespace argon::object::support;

ArObject *Formatter::NextArg() {
    if (AR_TYPEOF(this->fmt.args, type_tuple_)) {
        auto tp = ((Tuple *) this->fmt.args);

        this->fmt.args_len = tp->len;
        if (this->fmt.args_idx < this->fmt.args_len)
            return tp->objects[this->fmt.args_idx++];
    } else {
        this->fmt.args_len = 1;
        if (this->fmt.args_idx++ == 0)
            return this->fmt.args;
    }

    return ErrorFormat(type_type_error_, "not enough argument for format string");
}

ArSize Formatter::GetCapacity() {
    return this->cap_;
}

bool Formatter::BufferResize(ArSize sz) {
    ArSize csz = this->cap_;
    unsigned char *tmp;

    if (csz > 0)
        csz--;

    if (sz == 0 || this->idx_ + sz < csz)
        return true;

    if (this->str_ == nullptr)
        sz += 1;

    if ((tmp = ArObjectRealloc<unsigned char *>(this->str_, this->cap_ + sz)) == nullptr)
        return false;

    this->str_ = tmp;
    this->cap_ += sz;
    return true;
}

bool Formatter::DoFormat() {
    auto op = this->fmt.buf[this->fmt.idx];
    int result = -1;

    switch (op) {
        case 's':
            result = this->string_as_bytes ? this->FormatBytesString() : this->FormatString();
            break;
        case 'b':
            result = this->FormatInteger(2, false);
            break;
        case 'B':
            result = this->FormatInteger(2, true);
            break;
        case 'o':
            result = this->FormatInteger(8, false);
            break;
        case 'i':
        case 'd':
        case 'u':
            this->fmt.flags &= ~FormatFlags::ALT;
            result = this->FormatInteger(10, false);
            break;
        case 'x':
            result = this->FormatInteger(16, false);
            break;
        case 'X':
            result = this->FormatInteger(16, true);
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
            ErrorFormat(type_value_error_, "unsupported format character '%c' (0x%x)",
                        (31 <= op && op <= 126 ? '?' : op), op);
    }

    this->fmt.idx++;

    if (result < 0)
        return false;

    if (ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::LJUST))
        return this->WriteRepeat(' ', result);

    return true;
}

bool Formatter::ParseStarOption(bool prec) {
    const auto *num = (Integer *) this->NextArg();
    int opt;

    if (num == nullptr)
        return false;

    if (!AR_TYPEOF(num, type_integer_)) {
        ErrorFormat(type_type_error_, "* wants integer not '%s'", AR_TYPE_NAME(num));
        return false;
    }

    opt = (int) num->integer;

    if (opt < 0) {
        if (!prec)
            this->fmt.flags |= FormatFlags::LJUST;
        opt = -opt;
    }

    if (!prec)
        this->fmt.width = opt;
    else
        this->fmt.prec = opt;

    this->fmt.nspec++;
    return true;
}

bool Formatter::WriteRepeat(unsigned char chr, int times) {
    if (!this->BufferResize(times))
        return false;

    while (times-- > 0)
        this->str_[this->idx_++] = chr;

    return true;
}

int Formatter::FormatBytesString() {
    ArBuffer buffer = {};
    ArObject *obj;
    int wlen;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (!BufferGet(obj, &buffer, ArBufferFlags::READ))
        return -1;

    if ((wlen = this->Write(buffer.buffer, (int) buffer.len)) < 0) {
        BufferRelease(&buffer);
        return -1;
    }

    BufferRelease(&buffer);

    return wlen;
}

int Formatter::FormatChar() {
    unsigned char sequence[4] = {};
    ArObject *obj;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (AR_TYPEOF(obj, type_string_)) {
        const auto *s = (String *) obj;

        if (s->len > 1) {
            ErrorFormat(type_type_error_, "%%c requires a single char not string");
            return -1;
        }

        return this->Write(s->buffer, 1);
    } else if (AR_TYPEOF(obj, type_integer_)) {
        auto slen = StringIntToUTF8((int) ((Integer *) obj)->integer, sequence);

        if (slen == 0) {
            ErrorFormat(type_overflow_error_, "%%c arg not in range(0x110000)");
            return -1;
        }

        return this->Write(sequence, slen);
    }

    ErrorFormat(type_type_error_, "%%c requires integer or char not '%s'", AR_TYPE_NAME(obj));
    return -1;
}

int Formatter::FormatDecimal(unsigned char specifier) {
#define FMT_PRECISION_DEF   6
    ArObject *obj;
    DecimalUnderlying num;

    unsigned long intpart = 0;
    unsigned long frac = 0;
    long exp;

    int bufsz;
    int diff;
    int prec;

    bool upper = false;
    bool sn = false;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (AR_TYPEOF(obj, type_decimal_))
        num = ((Decimal *) obj)->decimal;
    else if (AR_TYPEOF(obj, type_integer_))
        num = (long double) ((Integer *) obj)->integer;
    else {
        ErrorFormat(type_type_error_, "%%%c requires real number not '%s'", this->fmt.buf[this->fmt.idx],
                    AR_TYPE_NAME(obj));
        return -1;
    }

    if (std::isnan(num)) // check NaN equals to num != num
        return this->Write((const unsigned char *) "nan", 3);
    else if (num > DBL_MAX)
        return this->Write((const unsigned char *) "+inf", 4);
    else if (num < -DBL_MAX)
        return this->Write((const unsigned char *) "-inf", 4);

    prec = this->fmt.prec > -1 ? this->fmt.prec : FMT_PRECISION_DEF;

    switch (specifier) {
        case 'E':
            upper = true;
            [[fallthrough]];
        case 'e':
            intpart = DecimalFrexp10(num, &frac, &exp, prec);
            sn = true;
            break;
        case 'G':
            upper = true;
            [[fallthrough]];
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

    if (num < 0 || ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::SIGN))
        bufsz++;

    if (ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::BLANK))
        bufsz++;

    if (frac > 0) {
        bufsz += IntegerCountDigits(frac, 10);
        bufsz++; // '.'
    }

    if (sn) {
        auto count = IntegerCountDigits(exp, 10);
        if (count <= 1)
            count += 2;
        bufsz += count + 2; // [+|-][e|E]
    }

    bufsz += this->fmt.width > bufsz ? this->fmt.width - bufsz : 0;

    if (!this->BufferResize(bufsz))
        return -1;

    diff = this->WriteNumber(this->str_ + this->idx_, (long) intpart, 10, 0, this->fmt.width, upper, this->fmt.flags);

    if (frac > 0) {
        this->str_[diff++ + this->idx_] = '.';
        diff += this->WriteNumber(this->str_ + this->idx_ + diff, (long) frac, 10, 0, 0, false, FormatFlags::NONE);
    }

    if (sn) {
        this->str_[diff++ + this->idx_] = upper ? 'E' : 'e';
        diff += this->WriteNumber(this->str_ + this->idx_ + diff, exp, 10, 2, 0, false, FormatFlags::SIGN);
    }

    this->idx_ += diff;

    return bufsz - diff;

#undef FMT_PRECISION_DEF
}

int Formatter::FormatInteger(int base, bool upper) {
    ArObject *obj;
    IntegerUnderlying num;
    int bufsz;
    int diff;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    if (!AR_TYPEOF(obj, type_integer_)) {
        ErrorFormat(type_type_error_, "%%%c requires integer not '%s'", this->fmt.buf[this->fmt.idx],
                    AR_TYPE_NAME(obj));
        return -1;
    }

    num = ((Integer *) obj)->integer;
    bufsz = IntegerCountDigits(num, base);

    if (this->fmt.prec > bufsz)
        bufsz = this->fmt.prec;

    if (num < 0 || ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::SIGN))
        bufsz++;

    if (ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::ALT))
        bufsz += 2;

    if (ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::BLANK))
        bufsz++;

    bufsz += this->fmt.width > bufsz ? this->fmt.width - bufsz : 0;

    if (!this->BufferResize(bufsz))
        return -1;

    diff = this->WriteNumber(this->str_ + this->idx_, num, base, this->fmt.prec > -1 ? this->fmt.prec : 0,
                             this->fmt.width, upper, this->fmt.flags);

    this->idx_ += diff;

    return bufsz - diff;
}

int
Formatter::FormatNumber(unsigned char *buf, int index, int base, int width, bool upper, bool neg, FormatFlags flags) {
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

int Formatter::FormatString() {
    ArObject *obj;
    String *s;
    ArSize slen;
    int ok;

    if ((obj = this->NextArg()) == nullptr)
        return -1;

    s = (String *) ToString(obj);
    slen = s->len;

    if (this->fmt.prec > -1 && slen > this->fmt.prec) {
        slen = s->kind != StringKind::ASCII ?
               StringSubStrLen(s, 0, slen) :
               this->fmt.prec;
    }

    ok = this->Write(s->buffer, (int) slen);
    Release(s);

    return ok;
}

int Formatter::ParseNextSpecifier() {
    const unsigned char *buf = this->fmt.buf + this->fmt.idx;
    int index = 0;

    while ((this->fmt.idx + index) < this->fmt.len) {
        if (buf[index++] == '%') {
            if ((this->fmt.idx + index) == this->fmt.len) {
                ErrorFormat(type_value_error_, "incomplete format specifier");
                return -1;
            }

            if (buf[index] != '%') {
                this->fmt.nspec++;
                this->fmt.idx++;
                index--;
                break;
            }

            index++;
        }
    }

    if (this->Write(buf, index) < 0)
        return -1;

    this->fmt.idx += index;

    return this->fmt.idx != this->fmt.len;
}

int Formatter::Write(const unsigned char *buf, int sz, int overalloc) {
    if (!this->BufferResize(sz + overalloc))
        return -1;

    memory::MemoryCopy(this->str_ + this->idx_, buf, sz);
    this->idx_ += sz;

    return sz;
}

int Formatter::WriteNumber(unsigned char *buf, long num, int base, int prec, int width, bool upper, FormatFlags flags) {
    static unsigned char l_case[] = "0123456789abcdef";
    static unsigned char u_case[] = "0123456789ABCDEF";
    const unsigned char *p_case = upper ? u_case : l_case;

    int index = 0;
    bool neg = false;

    if (num < 0) {
        num = -num;
        neg = true;
    } else if (num == 0)
        buf[index++] = '0';

    while (num) {
        buf[index++] = p_case[num % base];
        num /= base;
    }

    if (prec > index) {
        prec -= index;
        while (prec--)
            buf[index++] = '0';
    }

    return this->FormatNumber(buf, index, base, width, upper, neg, flags);
}

unsigned char *Formatter::format(ArSize *out_len) {
    unsigned char *buf;
    int ok;

    if (this->str_ != nullptr) {
        *out_len = this->idx_;
        return this->str_;
    }

    *out_len = 0;

    while ((ok = this->ParseNextSpecifier()) > 0) {
        this->ParseOption();

        if (!this->DoFormat())
            return nullptr;
    }

    if (ok < 0)
        return nullptr;

    if (this->fmt.nspec < this->fmt.args_len) {
        ErrorFormat(type_type_error_, "not all arguments converted during string formatting");
        return nullptr;
    }

    if (this->str_ != nullptr)
        this->str_[this->idx_] = '\0';

    buf = this->str_;
    *out_len = this->idx_;

    assert(this->idx_ < this->cap_);

    return buf;
}

void Formatter::ParseOption() {
    // Flags
    this->fmt.idx--;
    while (this->fmt.buf[this->fmt.idx++] > 0) {
        switch (this->fmt.buf[this->fmt.idx]) {
            case '-':
                this->fmt.flags |= FormatFlags::LJUST;
                continue;
            case '+':
                this->fmt.flags |= FormatFlags::SIGN;
                continue;
            case ' ':
                this->fmt.flags |= FormatFlags::BLANK;
                continue;
            case '#':
                this->fmt.flags |= FormatFlags::ALT;
                continue;
            case '0':
                this->fmt.flags |= FormatFlags::ZERO;
                continue;
            default:
                goto NEXT;
        }
    }

    NEXT:
    if (ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::LJUST) &&
        ENUMBITMASK_ISTRUE(this->fmt.flags, FormatFlags::ZERO))
        this->fmt.flags &= ~FormatFlags::ZERO;

    // Width
    this->fmt.width = 0;
    if (this->fmt.buf[this->fmt.idx] == '*') {
        this->fmt.idx++;
        if (!this->ParseStarOption(false))
            return;
    } else {
        while (isdigit(this->fmt.buf[this->fmt.idx])) {
            this->fmt.width = this->fmt.width * 10 + (this->fmt.buf[this->fmt.idx] - '0');
            this->fmt.idx++;
        }
    }

    // Precision
    this->fmt.prec = -1;
    if (this->fmt.buf[this->fmt.idx] == '.') {
        this->fmt.idx++;
        if (this->fmt.buf[this->fmt.idx] == '*') {
            this->fmt.idx++;
            if (!this->ParseStarOption(true))
                return;
        } else {
            this->fmt.prec = 0;
            while (isdigit(this->fmt.buf[this->fmt.idx])) {
                this->fmt.prec = this->fmt.prec * 10 + (this->fmt.buf[this->fmt.idx] - '0');
                this->fmt.idx++;
            }
        }
    }
}

void Formatter::ReleaseBufferOwnership() {
    this->str_ = nullptr;
    this->cap_ = 0;
    this->idx_ = 0;
}

Formatter::Formatter(const char *fmt, ArObject *args) {
    this->fmt.buf = (const unsigned char *) fmt;
    this->fmt.len = strlen(fmt);
    this->fmt.args = args;
}

Formatter::Formatter(const char *fmt, ArSize len, ArObject *args) {
    this->fmt.buf = (const unsigned char *) fmt;
    this->fmt.len = len;
    this->fmt.args = args;
}

Formatter::~Formatter() {
    argon::memory::Free(this->str_);
}
