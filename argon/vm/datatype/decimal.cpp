// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cfloat>
#include <cmath>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/hash_magic.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/decimal.h>

using namespace argon::vm::datatype;

// Prototypes

bool DecimalCanConvertFromInt(const Integer *integer, DecimalUnderlying *decimal);

// EOL

#define CONVERT_DOUBLE(object, dbl)                                             \
    if(AR_TYPEOF(object, type_decimal_))                                        \
        (dbl) = ((Decimal*)(object))->decimal;                                  \
    else if((!AR_TYPEOF(object, type_int_)  && !AR_TYPEOF(object, type_uint_))  \
            || !DecimalCanConvertFromInt(((Integer*)object), &dbl))             \
        return nullptr

#define SIMPLE_OP(left, right, op)  \
    DecimalUnderlying l;            \
    DecimalUnderlying r;            \
                                    \
    CONVERT_DOUBLE(left, l);        \
    CONVERT_DOUBLE(right, r);       \
                                    \
    return (ArObject*)DecimalNew(l op r)

ArObject *decimal_add(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, +);
}

ArObject *decimal_sub(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, -);
}

ArObject *decimal_mul(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, *);
}

ArObject *decimal_div(ArObject *left, ArObject *right) {
    DecimalUnderlying l;
    DecimalUnderlying r;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    return (ArObject *) DecimalNew(l / r);
}

ArObject *decimal_idiv(ArObject *left, ArObject *right) {
    DecimalUnderlying l;
    DecimalUnderlying r;
    DecimalUnderlying mod;
    DecimalUnderlying div;
    DecimalUnderlying floord;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    mod = fmod(l, r);
    div = (l - mod) / r;

    if (div) {
        floord = floor(div);
        if (div - floord > 0.5)
            floord += 1.0;
    } else
        floord = copysign(0.0, l / r);

    return (ArObject *) DecimalNew(floord);
}

ArObject *decimal_mod(ArObject *left, ArObject *right) {
    DecimalUnderlying l;
    DecimalUnderlying r;
    DecimalUnderlying mod;

    CONVERT_DOUBLE(left, l);
    CONVERT_DOUBLE(right, r);

    if (r == 0.0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if ((mod = fmod(l, r))) {
        // sign of remainder == sign of denominator
        if ((r < 0) != (mod < 0))
            mod += r;
    } else
        // remainder is zero, in case of signed zeroes fmod returns different result across platforms,
        // ensure it ha the same sign as the denominator
        mod = copysign(0.0, r);

    return (ArObject *) DecimalNew(mod);
}

ArObject *decimal_pos(Decimal *self) {
    if (self->decimal < 0)
        return (ArObject *) DecimalNew(-self->decimal);

    return (ArObject *) IncRef(self);
}

ArObject *decimal_neg(const Decimal *self) {
    return (ArObject *) DecimalNew(-self->decimal);
}

ArObject *decimal_inc(const Decimal *self) {
    return (ArObject *) DecimalNew(self->decimal + 1);
}

ArObject *decimal_dec(const Decimal *self) {
    return (ArObject *) DecimalNew(self->decimal - 1);
}

const OpSlots decimal_ops = {
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
        (UnaryOp) decimal_inc,
        (UnaryOp) decimal_dec
};

ArObject *decimal_compare(Decimal *self, ArObject *other, CompareMode mode) {
    DecimalUnderlying l = self->decimal;
    DecimalUnderlying r = .0;

    if ((ArObject *) self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (AR_TYPEOF(other, type_decimal_))
        r = ((Decimal *) other)->decimal;
    else if (AR_TYPEOF(other, type_int_))
        r = ((Integer *) other)->sint;
    else if (AR_TYPEOF(other, type_uint_))
        r = ((Integer *) other)->uint;

    ARGON_RICH_COMPARE_CASES(l, r, mode);
}

ArObject *decimal_repr(const Decimal *self) {
    return (ArObject *) StringFormat("%Lf", self->decimal);
}

// Hash of double number.
//
// From CPython implementation,
// see: https://docs.python.org/3/library/stdtypes.html section: Hashing of numeric types
// Source code: cpython/Python/pyhash.c
ArSize decimal_hash(const Decimal *self) {
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

    ArSize hash = 0;
    ArSize fp_tmp;
    while (float_part != 0) {
        hash = ((hash << (unsigned char) 28) & (ArSize) ARGON_OBJECT_HASH_PRIME) |
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

    hash = ((hash << (unsigned int) exponent) & (ArSize) ARGON_OBJECT_HASH_PRIME) |
           hash >> (unsigned char) (ARGON_OBJECT_HASH_BITS - exponent);

    hash = hash * sign;

    if (hash == -1)
        hash = -2;

    return hash;
}

bool decimal_is_true(const Decimal *self) {
    return self->decimal > 0;
}

TypeInfo DecimalType = {
        AROBJ_HEAD_INIT_TYPE,
        "Decimal",
        nullptr,
        nullptr,
        sizeof(Decimal),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) decimal_hash,
        (Bool_UnaryOp) decimal_is_true,
        (CompareOp) decimal_compare,
        (UnaryConstOp) decimal_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &decimal_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_decimal_ = &DecimalType;

Decimal *argon::vm::datatype::DecimalNew(DecimalUnderlying number) {
    auto *decimal = MakeObject<Decimal>(type_decimal_);

    if (decimal != nullptr)
        decimal->decimal = number;

    return decimal;
}

Decimal *argon::vm::datatype::DecimalNew(const char *string) {
    auto *decimal = MakeObject<Decimal>(type_decimal_);

    if (decimal != nullptr)
        decimal->decimal = std::strtod(string, nullptr);

    return decimal;
}

bool DecimalCanConvertFromInt(const Integer *integer, DecimalUnderlying *decimal) {
    DecimalUnderlying ipart;
    int exp;

    if (AR_TYPEOF(integer, type_int_))
        ipart = frexp(integer->sint, &exp);
    else
        ipart = frexpl(integer->uint, &exp);

    if (exp > DBL_MAX_EXP) {
        ErrorFormat(kOverflowError[0], "integer too large to convert to decimal");
        return false;
    }

    *decimal = ldexp(ipart, exp);
    return true;
}