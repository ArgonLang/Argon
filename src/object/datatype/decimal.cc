// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>
#include <cassert>

#include "hash_magic.h"
#include "integer.h"
#include "decimal.h"
#include "bool.h"

using namespace argon::object;

ArObject *decimal_add(ArObject *left, ArObject *right) {
    auto d = (Decimal *) left;

    if (left->type == &type_decimal_) {
        if (right->type == left->type)
            return DecimalNew(d->decimal + ((Decimal *) right)->decimal);
        else if (right->type == &type_integer_)
            return DecimalNew(d->decimal + ((Integer *) right)->integer);
    } else if (left->type == &type_integer_) {
        d = (Decimal *) right;
        return DecimalNew(((Integer *) right)->integer + d->decimal);
    }

    return nullptr;
}

ArObject *decimal_sub(ArObject *left, ArObject *right) {
    auto d = (Decimal *) left;

    if (left->type == &type_decimal_) {
        if (right->type == left->type)
            return DecimalNew(d->decimal - ((Decimal *) right)->decimal);
        else if (right->type == &type_integer_)
            return DecimalNew(d->decimal - ((Integer *) right)->integer);
    } else if (left->type == &type_integer_) {
        d = (Decimal *) right;
        return DecimalNew(((Integer *) right)->integer - d->decimal);
    }

    return nullptr;
}

ArObject *decimal_mul(ArObject *left, ArObject *right) {
    auto d = (Decimal *) left;

    if (left->type == &type_decimal_) {
        if (right->type == left->type)
            return DecimalNew(d->decimal * ((Decimal *) right)->decimal);
        else if (right->type == &type_integer_)
            return DecimalNew(d->decimal * ((Integer *) right)->integer);
    } else if (left->type == &type_integer_) {
        d = (Decimal *) right;
        return DecimalNew(((Integer *) right)->integer * d->decimal);
    }

    return nullptr;
}

ArObject *decimal_div(ArObject *left, ArObject *right) {
    auto l = (Decimal *) left;

    if (left->type == &type_decimal_) {
        if (right->type == &type_integer_) {
            if (((Integer *) right)->integer == 0)
                return argon::vm::Panic(ZeroDivisionError);
            return DecimalNew((DecimalUnderlayer) l->decimal / ((Integer *) right)->integer);
        } else if (left->type == right->type) {
            if (((Decimal *) right)->decimal == 0)
                return argon::vm::Panic(ZeroDivisionError);
            return DecimalNew((DecimalUnderlayer) l->decimal / ((Decimal *) right)->decimal);
        }
    }

    return nullptr;
}

ArObject *decimal_idiv(ArObject *left, ArObject *right) {
    if (left->type == right->type) {
        if (((Decimal *) right)->decimal == 0)
            return argon::vm::Panic(ZeroDivisionError);

        return IntegerNew((IntegerUnderlayer) (((Decimal *) left)->decimal / ((Decimal *) right)->decimal));
    } else if (right->type == &type_integer_) {
        if (((Integer *) right)->integer == 0)
            return argon::vm::Panic(ZeroDivisionError);

        return IntegerNew((IntegerUnderlayer) (((Decimal *) left)->decimal / ((Integer *) right)->integer));
    }

    return nullptr;
}

ArObject *decimal_mod(ArObject *self, ArObject *other) {
    DecimalUnderlayer mod;

    if (self->type == other->type) {
        mod = fmod(((Decimal *) self)->decimal, ((Decimal *) other)->decimal);
        return DecimalNew(mod);
    }

    return nullptr;
}

ArObject *decimal_pos(Decimal *self) {
    if (self->decimal < 0)
        return DecimalNew(self->decimal * -1);
    IncRef(self);
    return self;
}

ArObject *decimal_neg(Decimal *self) {
    if (self->decimal > 0)
        return DecimalNew(self->decimal * -1);
    IncRef(self);
    return self;
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
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

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

ArObject *decimal_compare(ArObject *self, ArObject *other, CompareMode mode) {
    DecimalUnderlayer l = ((Decimal *) self)->decimal;
    DecimalUnderlayer r;
    IntegerUnderlayer integer;

    if (other->type == &type_decimal_)
        r = ((Decimal *) other)->decimal;
    else if (other->type == &type_integer_) {
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

bool decimal_equal(ArObject *self, ArObject *other) {
    DecimalUnderlayer l = ((Decimal *) self)->decimal;

    if (self != other) {
        if (self->type == other->type)
            return l == ((Decimal *) other)->decimal;
        else if (other->type == &type_integer_)
            return l == ((Integer *) other)->integer;

        return false;
    }

    return true;
}

bool decimal_istrue(Decimal *self) {
    return self->decimal > 0;
}

const TypeInfo argon::object::type_decimal_ = {
        (const unsigned char *) "decimal",
        sizeof(Decimal),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) decimal_istrue,
        decimal_equal,
        decimal_compare,
        decimal_hash,
        nullptr,
        &decimal_ops,
        nullptr,
        nullptr
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
