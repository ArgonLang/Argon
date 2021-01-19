// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>

#include "bool.h"
#include "decimal.h"
#include "error.h"

#include "integer.h"

using namespace argon::object;

#define CHECK_INTEGER(left, right)                                          \
    if(!AR_TYPEOF(left, type_integer_) || !AR_TYPEOF(right, type_integer_)) \
        return nullptr

#define SIMPLE_OP(left, right, op)                                                  \
    CHECK_INTEGER(left, right);                                                     \
    return IntegerNew(((Integer*)(left))->integer op ((Integer*)(right))->integer)

ArObject *integer_as_integer(Integer *self) {
    IncRef(self);
    return self;
}

ArSSize integer_as_index(Integer *self) {
    return self->integer;
}

const NumberSlots integer_nslots{
        (UnaryOp) integer_as_integer,
        (ArSizeUnaryOp) integer_as_index
};

ArObject *integer_add(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, +);
}

ArObject *integer_sub(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, -);
}

ArObject *integer_mul(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, *);
}

ArObject *integer_div(ArObject *left, ArObject *right) {
    CHECK_INTEGER(left, right);

    if (((Integer *) right)->integer == 0)
        return argon::vm::Panic(ZeroDivisionError);

    return IntegerNew(((Integer *) (left))->integer / ((Integer *) (right))->integer);
}

ArObject *integer_mod(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, %);
}

ArObject *integer_pos(Integer *self) {
    if (self->integer < 0)
        return IntegerNew(-self->integer);
    IncRef(self);
    return self;
}

ArObject *integer_neg(Integer *self) {
    return IntegerNew(-self->integer);
}

ArObject *integer_land(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, &);
}

ArObject *integer_lor(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, |);
}

ArObject *integer_lxor(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, ^);
}

ArObject *integer_lsh(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, <<);
}

ArObject *integer_rsh(ArObject *left, ArObject *right) {
    SIMPLE_OP(left, right, >>);
}

ArObject *integer_inv(Integer *self) {
    return IntegerNew(~self->integer);
}

ArObject *integer_inc(Integer *self) {
    return IntegerNew(self->integer + 1);
}

ArObject *integer_dec(Integer *self) {
    return IntegerNew(self->integer - 1);
}

#undef SIMPLE_OP

const OpSlots integer_ops{
        integer_add,
        integer_sub,
        integer_mul,
        integer_div,
        integer_div,
        integer_mod,
        (UnaryOp) integer_pos,
        (UnaryOp) integer_neg,
        integer_land,
        integer_lor,
        integer_lxor,
        (BinaryOp) integer_lsh,
        (BinaryOp) integer_rsh,
        (UnaryOp) integer_inv,
        integer_add,
        integer_sub,
        integer_mul,
        integer_div,
        (UnaryOp) integer_inc,
        (UnaryOp) integer_dec,
};

bool integer_is_true(Integer *self) {
    return self->integer > 0;
}

bool integer_equal(Integer *self, ArObject *other) {
    if (self == other)
        return true;

    if (!AR_TYPEOF(self, type_integer_) || !AR_TYPEOF(other, type_integer_))
        return false;

    return self->integer == ((Integer *) other)->integer;
}

ArObject *integer_compare(Integer *self, ArObject *other, CompareMode mode) {
    IntegerUnderlayer left = self->integer;
    IntegerUnderlayer right;

    if (AR_SAME_TYPE(self, other)) {
        right = ((Integer *) other)->integer;

        switch (mode) {
            case CompareMode::GE:
                return BoolToArBool(left > right);
            case CompareMode::GEQ:
                return BoolToArBool(left >= right);
            case CompareMode::LE:
                return BoolToArBool(left < right);
            case CompareMode::LEQ:
                return BoolToArBool(left <= right);
            default:
                assert(false); // Never get here!
        }
    }

    return nullptr;
}

size_t integer_hash(ArObject *obj) {
    return ((Integer *) obj)->integer;
}

ArObject *integer_str(Integer *self) {
    return StringCFormat("%i", self);
}

const TypeInfo argon::object::type_integer_ = {
        TYPEINFO_STATIC_INIT,
        "integer",
        nullptr,
        sizeof(Integer),
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) integer_compare,
        (BoolBinOp) integer_equal,
        (BoolUnaryOp) integer_is_true,
        integer_hash,
        (UnaryOp) integer_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &integer_nslots,
        nullptr,
        nullptr,
        &integer_ops
};

Integer *argon::object::IntegerNew(IntegerUnderlayer number) {
    // Overflow check
#if __WORDSIZE == 32
    if (number > 0x7FFFFFFF)
        return (Integer *) ErrorFormat(&error_overflow_error, "integer too large to be represented by signed C long");
#elif __WORDSIZE == 64
    if (number > 0x7FFFFFFFFFFFFFFF)
        return (Integer *) ErrorFormat(&error_overflow_error, "integer too large to be represented by signed C long");
#endif

    auto integer = ArObjectNew<Integer>(RCType::INLINE, &type_integer_);

    if (integer != nullptr)
        integer->integer = number;

    return integer;
}

Integer *argon::object::IntegerNewFromString(const std::string &string, int base) {
    auto integer = ArObjectNew<Integer>(RCType::INLINE, &type_integer_);

    if (integer != nullptr)
        integer->integer = std::strtol(string.c_str(), nullptr, base);

    return integer;
}

int argon::object::IntegerCountBits(Integer *number) {
    IntegerUnderlayer i = number->integer;
    int count = 0;

    if (i < 0)i *= -1;

    while (i) {
        count++;
        i >>= 1u;
    }

    return count;
}

int argon::object::IntegerCountDigits(IntegerUnderlayer number, IntegerUnderlayer base) {
    int count = 0;

    while (number) {
        count++;
        number /= base;
    }

    return count;
}

#undef CHECK_INTEGER