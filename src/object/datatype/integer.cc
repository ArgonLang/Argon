// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <utils/macros.h>

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

ARGON_FUNCTION5(integer_, new, "Convert a string or number to decimal numer, if possible."
                               ""
                               "- Parameter obj: obj to convert."
                               "- Returns: decimal number.", 1, true) {
    ArBuffer buffer = {};
    IntegerUnderlying num = 0;
    int base = 10;

    if (!VariadicCheckPositional("integer::new", count, 1, 2))
        return nullptr;

    if (count == 2) {
        if (!AR_TYPEOF(argv[1], type_integer_))
            return ErrorFormat(type_type_error_, "base must be an integer not '%s'", AR_TYPE_NAME(argv[1]));
        base = ((Integer *) argv[1])->integer;
    }

    if (count >= 1) {
        if (AR_TYPEOF(*argv, type_integer_))
            return IncRef(*argv);
        else if (AR_TYPEOF(*argv, type_decimal_))
            num = ((Decimal *) *argv)->decimal;
        else if (IsBufferable(*argv)) {
            if (!BufferGet(*argv, &buffer, ArBufferFlags::READ))
                return nullptr;

            num = std::strtol((char *) buffer.buffer, nullptr, base);

            BufferRelease(&buffer);
        } else
            return ErrorFormat(type_not_implemented_, "no viable conversion from '%s' to '%s'",
                               AR_TYPE_NAME(*argv), type_integer_->name);
    }

    return IntegerNew(num);
}

ARGON_METHOD5(integer_, bits, "Return number of bits necessary to represent an integer in binary."
                              ""
                              "- Returns: number of bits(integer).", 0, false) {
    return IntegerNew(IntegerCountBits(((Integer*)self)));
}

const NativeFunc integer_methods[] = {
        integer_bits_,
        integer_new_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots integer_obj{
        integer_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
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
        return argon::vm::Panic(error_zero_division);

    return IntegerNew(((Integer *) (left))->integer / ((Integer *) (right))->integer);
}

ArObject *integer_mod(ArObject *left, ArObject *right) {
    IntegerUnderlying l;
    IntegerUnderlying r;
    IntegerUnderlying ans;

    CHECK_INTEGER(left, right);

    l = ((Integer *) left)->integer;
    r = ((Integer *) right)->integer;

    ans = l % r;
    if (ans < 0)
        ans += r;

    return IntegerNew(ans);
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

ArObject *integer_compare(Integer *self, ArObject *other, CompareMode mode) {
    IntegerUnderlying left = self->integer;
    IntegerUnderlying right;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    right = ((Integer *) other)->integer;

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArSize integer_hash(ArObject *obj) {
    return ((Integer *) obj)->integer;
}

ArObject *integer_str(Integer *self) {
    return StringCFormat("%i", self);
}

const TypeInfo IntegerType = {
        TYPEINFO_STATIC_INIT,
        "integer",
        nullptr,
        sizeof(Integer),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) integer_compare,
        (BoolUnaryOp) integer_is_true,
        integer_hash,
        (UnaryOp) integer_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &integer_nslots,
        &integer_obj,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_integer_ = &IntegerType;

Integer *argon::object::IntegerNew(IntegerUnderlying number) {
    // Overflow check
#if _ARGON_ENVIRON == 32
    if (number > 0x7FFFFFFF)
        return (Integer *) ErrorFormat(type_overflow_error_, "integer too large to be represented by signed C long");
#else
    if (number > 0x7FFFFFFFFFFFFFFF)
        return (Integer *) ErrorFormat(type_overflow_error_, "integer too large to be represented by signed C long");
#endif

    auto integer = ArObjectNew<Integer>(RCType::INLINE, type_integer_);

    if (integer != nullptr)
        integer->integer = number;

    return integer;
}

Integer *argon::object::IntegerNewFromString(const std::string &string, int base) {
    auto integer = ArObjectNew<Integer>(RCType::INLINE, type_integer_);

    if (integer != nullptr)
        integer->integer = std::strtol(string.c_str(), nullptr, base);

    return integer;
}

int argon::object::IntegerCountBits(Integer *number) {
    IntegerUnderlying i = number->integer;
    int count = 0;

    if (i < 0)i *= -1;

    while (i) {
        count++;
        i >>= 1u;
    }

    return count;
}

int argon::object::IntegerCountDigits(IntegerUnderlying number, IntegerUnderlying base) {
    int count = 0;

    if (number == 0)
        return 1;

    while (number) {
        count++;
        number /= base;
    }

    return count;
}

#undef CHECK_INTEGER