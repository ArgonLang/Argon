// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>
#include <cassert>

#include <vm/runtime.h>

#include "hash_magic.h"
#include "bool.h"
#include "error.h"
#include "integer.h"

#include "decimal.h"

using namespace argon::object;

#define CONVERT_DOUBLE(object, dbl)             \
    if(AR_TYPEOF(object, type_decimal_))        \
        (dbl) = ((Decimal*)(object))->decimal;  \
    else if(!ConvertToDouble(object, &(dbl)))   \
        return nullptr;

#define SIMPLE_OP(left, right, op)  \
    DecimalUnderlayer l;            \
    DecimalUnderlayer r;            \
                                    \
    CONVERT_DOUBLE(left, l);        \
    CONVERT_DOUBLE(right, r);       \
                                    \
    return DecimalNew(l op r)

bool ConvertToDouble(ArObject *obj, DecimalUnderlayer *decimal) {
    DecimalUnderlayer ipart;
    int exp;

    if (AR_TYPEOF(obj, type_integer_)) {
        ipart = frexp(((Integer *) obj)->integer, &exp);
        if (exp > DBL_MAX_EXP) {
            ErrorFormat(&error_overflow_error, "integer too large to convert to decimal");
            return false;
        }
        *decimal = ldexp(ipart, exp);
        return true;
    }

    return false;
}

ArObject *decimal_as_integer(Decimal *self) {
    IntegerUnderlayer num;
    unsigned long exp;

    if (std::isinf(self->decimal))
        return ErrorFormat(&error_overflow_error, "cannot convert infinity to integer");
    if (std::isnan(self->decimal))
        return ErrorFormat(&error_overflow_error, "cannot convert NaN to integer");

    num = DecimalModf(self->decimal, &exp, 0);

    if (self->decimal < 0)
        num = -num;

    return IntegerNew(num);
}

const NumberSlots decimal_nslots = {
        (UnaryOp) decimal_as_integer,
        nullptr
};

ArObject *decimal_add(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, +);
}

ArObject *decimal_sub(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, -);
}

ArObject *decimal_mul(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, *);
}

#undef SIMPLE_OP

ArObject *decimal_div(ArObject *left, ArObject *right) {
    DecimalUnderlayer l;
    DecimalUnderlayer r;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0)
        return argon::vm::Panic(ZeroDivisionError);

    return DecimalNew(l / r);
}

ArObject *decimal_idiv(ArObject *left, ArObject *right) {
    DecimalUnderlayer l;
    DecimalUnderlayer r;
    DecimalUnderlayer mod;
    DecimalUnderlayer div;
    DecimalUnderlayer floord;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0)
        return argon::vm::Panic(ZeroDivisionError);

    mod = fmod(l, r);
    div = (l - mod) / r;

    if (div) {
        floord = floor(div);
        if (div - floord > 0.5)
            floord += 1.0;
    } else
        floord = copysign(0.0, l / r);

    return DecimalNew(floord);
}

ArObject *decimal_mod(ArObject *left, ArObject *right) {
    DecimalUnderlayer l;
    DecimalUnderlayer r;
    DecimalUnderlayer mod;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0)
        return argon::vm::Panic(ZeroDivisionError);

    if ((mod = fmod(l, r))) {
        // sign of remainder == sign of denominator
        if ((r < 0) != (mod < 0))
            mod += r;
    } else
        // remainder is zero, in case of signed zeroes fmod returns different result across platforms,
        // ensure it ha the same sign as the denominator
        mod = copysign(0.0, r);

    return DecimalNew(mod);
}

ArObject *decimal_pos(Decimal *self) {
    if (self->decimal < 0)
        return DecimalNew(-self->decimal);
    IncRef(self);
    return self;
}

ArObject *decimal_neg(Decimal *self) {
    return DecimalNew(-self->decimal);
}

ArObject *decimal_inc(Decimal *self) {
    return DecimalNew(self->decimal + 1);
}

ArObject *decimal_dec(Decimal *self) {
    return DecimalNew(self->decimal - 1);
}

const OpSlots decimal_ops{
        decimal_add,
        decimal_sub,
        decimal_mul,
        decimal_div,
        decimal_idiv,
        decimal_mod,
        (UnaryOp) decimal_pos,
        (UnaryOp) decimal_neg,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        decimal_add,
        decimal_sub,
        decimal_mul,
        decimal_div,
        (UnaryOp) decimal_inc,
        (UnaryOp) decimal_dec
};

bool decimal_is_true(Decimal *self) {
    return self->decimal > 0;
}

ArObject *decimal_compare(Decimal *self, ArObject *other, CompareMode mode) {
    DecimalUnderlayer l = self->decimal;
    DecimalUnderlayer r;

    IntegerUnderlayer integer;

    if (AR_TYPEOF(other, type_decimal_))
        r = ((Decimal *) other)->decimal;
    else if (AR_TYPEOF(other, type_integer_)) {
        integer = ((Integer *) other)->integer;

        if (std::isfinite(l)) {
            int lsign = l == 0 ? 0 : l < 0 ? -1 : 1;
            int rsign = integer == 0 ? 0 : integer < 0 ? -1 : 1;
            if (lsign != rsign) {
                // The signs are enough to tell the difference.
                l = lsign;
                r = rsign;
            } else {
                // Move integer into a double if it fits in.
                auto num_bits = IntegerCountBits((Integer *) other);
                if (num_bits <= 52) {
                    r = ((Integer *) other)->integer;
                } else {
                    // The number is larger than 52 bits
                    int exp;
                    frexp(l, &exp);
                    if (exp < 0 || exp < num_bits) {
                        l = 1.0;
                        r = 2.0;
                    } else if (exp > num_bits) {
                        l = 2.0;
                        r = 1.0;
                    } else {
                        // Same size
                        double intpart, fractpart;
                        fractpart = modf(l, &intpart);

                        if ((IntegerUnderlayer) intpart == ((Integer *) other)->integer) {
                            l = r = 1;
                            if (fractpart > 0.0)
                                l++;
                            else if (fractpart < 0.0)
                                l--;
                        } else if ((IntegerUnderlayer) intpart > ((Integer *) other)->integer) {
                            l = 2.0;
                            r = 1.0;
                        } else {
                            l = 1.0;
                            r = 2.0;
                        }
                    }
                }
            }
        } else r = .0;
    } else
        return ErrorFormat(&error_type_error,
                           "unsupported operation between type '%s' and type '%s'",
                           self->type->name,
                           other->type->name);

    switch (mode) {
        case CompareMode::EQ:
            return BoolToArBool(l == r);
        case CompareMode::GE:
            return BoolToArBool(l > r);
        case CompareMode::GEQ:
            return BoolToArBool(l >= r);
        case CompareMode::LE:
            return BoolToArBool(l < r);
        case CompareMode::LEQ:
            return BoolToArBool(l <= r);
        default:
            assert(false); // Never get here!
    }
}

bool decimal_equal(Decimal *self, ArObject *other) {
    Bool *eq;

    if (self == other)
        return true;

    if ((eq = (Bool *) decimal_compare(self, other, CompareMode::EQ)) == True) {
        Release(eq);
        return true;
    }

    Release(eq);
    return false;
}

// Hash of double number.
//
// From CPython implementation,
// see: https://docs.python.org/3/library/stdtypes.html section: Hashing of numeric types
// Source code: cpython/Python/pyhash.c
size_t decimal_hash(Decimal *self) {
    long double float_part;
    int sign = 1;
    int exponent;

    if (std::isnan(self->decimal))
        return ARGON_OBJECT_HASH_NAN;
    else if (std::isinf(self->decimal))
        return ARGON_OBJECT_HASH_INF;

    float_part = std::frexp(self->decimal, &exponent); // decimal_ = float_part * pow(2,exponent)

    if (float_part < 0) {
        sign = -1;
        float_part = -float_part;
    }

    size_t hash = 0;
    size_t fp_tmp;
    while (float_part != 0) {
        hash = ((hash << (unsigned char) 28) & (size_t) ARGON_OBJECT_HASH_PRIME) |
               hash >> (unsigned char) (ARGON_OBJECT_HASH_BITS - 28);
        float_part *= 268435456.f;
        exponent -= 28;
        fp_tmp = float_part;
        float_part -= fp_tmp;
        hash += fp_tmp;
        if (hash >= ARGON_OBJECT_HASH_PRIME)
            hash -= ARGON_OBJECT_HASH_PRIME;
    }

    if (exponent >= 0)
        exponent %= ARGON_OBJECT_HASH_BITS;
    else
        exponent = ARGON_OBJECT_HASH_BITS - 1 - ((-1 - exponent) % ARGON_OBJECT_HASH_BITS);

    hash = ((hash << (unsigned int) exponent) & (size_t) ARGON_OBJECT_HASH_PRIME) |
           hash >> (unsigned char) (ARGON_OBJECT_HASH_BITS - exponent);

    hash = hash * sign;

    if (hash == -1)
        hash = -2;

    return hash;
}

ArObject *decimal_str(Decimal *self) {
    return StringCFormat("%f", self);
}

const TypeInfo argon::object::type_decimal_ = {
        TYPEINFO_STATIC_INIT,
        "decimal",
        nullptr,
        sizeof(Decimal),
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) decimal_compare,
        (BoolBinOp) decimal_equal,
        (BoolUnaryOp) decimal_is_true,
        (SizeTUnaryOp) decimal_hash,
        (UnaryOp) decimal_str,
        nullptr,
        nullptr,
        &decimal_nslots,
        nullptr,
        nullptr,
        &decimal_ops
};

Decimal *argon::object::DecimalNew(DecimalUnderlayer number) {
    auto decimal = ArObjectNew<Decimal>(RCType::INLINE, &type_decimal_);
    if (decimal != nullptr)
        decimal->decimal = number;
    return decimal;
}

Decimal *argon::object::DecimalNewFromString(const std::string &string) {
    auto decimal = ArObjectNew<Decimal>(RCType::INLINE, &type_decimal_);
    std::size_t idx;

    if (decimal != nullptr)
        decimal->decimal = std::stold(string, &idx);

    return decimal;
}

unsigned long argon::object::DecimalModf(DecimalUnderlayer value, unsigned long *frac, int precision) {
    static int pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

    IntegerUnderlayer intpart = value;
    DecimalUnderlayer tmp;
    DecimalUnderlayer diff;

    // based on https://github.com/mpaland/printf (_ftoa)

    if (std::isnan(value) || std::isinf(value)) {
        *frac = 0;
        return 0;
    }

    if (value < 0)
        value = -value;

    // Limit precision to 9
    if (precision > 9)
        precision = 9;

    tmp = (value - intpart) * pow10[precision];
    *frac = (unsigned long) tmp;
    diff = tmp - *frac;

    if (diff > 0.5) {
        (*frac)++;
        // handle rollover, e.g. 0.99 with precision = 1 is 1.0
        if (*frac >= pow10[precision]) {
            *frac = 0;
            intpart++;
        }
    } else if (diff < 0.5) {

    } else if ((*frac == 0U) || (*frac & 1U))
        (*frac)++; // round up if odd OR last digit is 0

    if (precision == 0U) {
        diff = value - (DecimalUnderlayer) intpart;
        if (diff >= 0.5 && ((unsigned long) intpart & 1U))
            intpart++; // round up if exactly 0.5 and odd
    }

    return intpart;
}

unsigned long argon::object::DecimalFrexp10(DecimalUnderlayer value, unsigned long *frac, long *exp, int precision) {
    int exp2;
    double z;
    double z2;

    union {
        double F;
        uint64_t U;
    } conv{};

    // based on https://github.com/mpaland/printf (_etoa)
    conv.F = value;

    exp2 = (int) ((conv.U >> 52U) & 0x07FFU) - 1023;                // effectively log2
    conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);    // drop the exponent so conv.F is now in [1,2)

    // now approximate log10 from the log2 integer part and an expansion of ln around 1.5
    (*exp) = (int) (0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);

    // now we want to compute 10^(expval) but we want to be sure it won't overflow
    exp2 = (*exp) * 3.321928094887362 + 0.5;
    z = (*exp) * 2.302585092994046 - exp2 * 0.6931471805599453;
    z2 = z * z;
    conv.U = (uint64_t) (exp2 + 1023) << 52U;

    // compute exp(z) using continued fractions,
    conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));

    // correct for rounding errors
    if (value < conv.F) {
        (*exp)--;
        conv.F /= 10;
    }

    // rescale the float value
    if ((*exp))
        value /= conv.F;

    return DecimalModf(value, frac, precision);
}

#undef CONVERT_DOUBLE