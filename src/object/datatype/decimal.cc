// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>
#include <cassert>
#include "decimal.h"
#include "hash_magic.h"

using namespace argon::object;

// Hash of double number.
//
// From CPython implementation,
// see: https://docs.python.org/3/library/stdtypes.html section: Hashing of numeric types
// Source code: cpython/Python/pyhash.c

size_t decimal_hash(ArObject *obj) {
    long double float_part;
    int sign = 1;
    int exponent;

    auto self = (Decimal *) obj;

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

bool decimal_equal(ArObject *self, ArObject *other) {
    if (self != other) {
        if (self->type == other->type)
            return ((Decimal *) self)->decimal == ((Decimal *) other)->decimal;
        return false;
    }
    return true;
}

bool decimal_istrue(Decimal *self) {
    return self->decimal > 0;
}

const NumberActions decimal_actions{
        nullptr
};

const TypeInfo type_decimal_ = {
        (const unsigned char *) "decimal",
        sizeof(Decimal),
        &decimal_actions,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) decimal_istrue,
        decimal_equal,
        nullptr,
        decimal_hash,
        nullptr,
        nullptr,
        nullptr
};

Decimal *argon::object::DecimalNew(long double number) {
    auto decimal = ArObjectNew<Decimal>(RCType::INLINE, &type_decimal_);
    assert(decimal != nullptr);
    decimal->decimal = number;
    return decimal;
}

Decimal *argon::object::DecimalNewFromString(const std::string &string) {
    auto decimal = ArObjectNew<Decimal>(RCType::INLINE, &type_decimal_);
    std::size_t idx;
    assert(decimal != nullptr);
    decimal->decimal = std::stold(string, &idx);
    return decimal;
}