// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>
#include "error.h"
#include "bool.h"
#include "decimal.h"
#include "integer.h"

using namespace argon::object;

ArObject *integer_add(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer + ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_sub(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer - ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_mul(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer * ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_div(ArObject *left, ArObject *right) {
    auto l = (Integer *) left;

    if (left->type == &type_integer_) {
        if (right->type == &type_decimal_) {
            if (((Decimal *) right)->decimal == 0)
                return argon::vm::Panic(ZeroDivisionError);
            return DecimalNew((DecimalUnderlayer) l->integer / ((Decimal *) right)->decimal);
        } else if (left->type == right->type) {
            if (((Integer *) right)->integer == 0)
                return argon::vm::Panic(ZeroDivisionError);
            return DecimalNew((DecimalUnderlayer) l->integer / ((Integer *) right)->integer);
        }
    }

    return nullptr;
}

ArObject *integer_idiv(ArObject *left, ArObject *right) {
    if (left->type == right->type) {
        if (((Integer *) right)->integer == 0)
            return argon::vm::Panic(ZeroDivisionError);
        return IntegerNew(((Integer *) left)->integer / ((Integer *) right)->integer);
    } else if (right->type == &type_decimal_) {
        if (((Decimal *) right)->decimal == 0)
            return argon::vm::Panic(ZeroDivisionError);
        return IntegerNew((((Integer *) left)->integer / ((Decimal *) right)->decimal));
    }

    return nullptr;
}

ArObject *integer_mod(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer % ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_pos(Integer *self) {
    if (self->integer < 0)
        return IntegerNew(self->integer * -1);
    IncRef(self);
    return self;
}

ArObject *integer_neg(Integer *self) {
    if (self->integer > 0)
        return IntegerNew(-self->integer);
    IncRef(self);
    return self;
}

ArObject *integer_land(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer & ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_lor(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer | ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_lxor(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer ^ ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_lsh(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer << ((Integer *) right)->integer);
    return nullptr;
}

ArObject *integer_rsh(ArObject *left, ArObject *right) {
    if (left->type == right->type)
        return IntegerNew(((Integer *) left)->integer >> ((Integer *) right)->integer);
    return nullptr;
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

arsize integer_as_index(Integer *self) {
    return self->integer;
}

const NumberActions integer_actions{
        nullptr,
        (ArSizeUnaryOp) integer_as_index
};

const OpSlots integer_ops{
        integer_add,
        integer_sub,
        integer_mul,
        integer_div,
        integer_idiv,
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

bool integer_equal(ArObject *self, ArObject *other) {
    IntegerUnderlayer i = ((Integer *) self)->integer;

    if (self != other) {
        if (self->type == other->type)
            return i == ((Integer *) other)->integer;
        else if (other->type == &type_decimal_)
            return i == ((Decimal *) other)->decimal;

        return false;
    }

    return true;
}

ArObject *integer_compare(ArObject *self, ArObject *other, CompareMode mode) {
    if (self->type == other->type) {
        IntegerUnderlayer left = ((Integer *) self)->integer;
        IntegerUnderlayer right = ((Integer *) other)->integer;
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

bool integer_istrue(Integer *self) {
    return self->integer > 0;
}

const TypeInfo argon::object::type_integer_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "integer",
        sizeof(Integer),
        nullptr,
        &integer_actions,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) integer_istrue,
        integer_equal,
        integer_compare,
        integer_hash,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
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
    assert(number->type == &type_integer_);

    IntegerUnderlayer i = number->integer;
    int count = 0;

    if (i < 0)i *= -1;

    while (i) {
        count++;
        i >>= 1;
    }

    return count;
}