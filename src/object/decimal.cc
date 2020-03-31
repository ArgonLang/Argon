// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>
#include "decimal.h"
#include "hash_magic.h"

using namespace argon::object;

Decimal::Decimal(long double number) : Number(&type_decimal_), decimal_(number) {}

Decimal::Decimal(const std::string &number) : Number(&type_decimal_) {
    std::size_t idx;
    this->decimal_ = std::stold(number, &idx);
}

bool Decimal::EqualTo(const Object *other) {
    if (this != other) {
        if (this->type == other->type)
            return this->decimal_ == ((Decimal *) other)->decimal_;
        return false;
    }

    return true;
}

// Hash of double number.
//
// From CPython implementation,
// see: https://docs.python.org/3/library/stdtypes.html section: Hashing of numeric types
// Source code: cpython/Python/pyhash.c

size_t Decimal::Hash() {
    long double float_part;
    int sign = 1;
    int exponent;


    if (std::isnan(this->decimal_))
        return ARGON_OBJECT_HASH_NAN;
    else if (std::isinf(this->decimal_))
        return ARGON_OBJECT_HASH_INF;

    float_part = std::frexp(this->decimal_, &exponent); // decimal_ = float_part * pow(2,exponent)

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
