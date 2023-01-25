// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "boolean.h"
#include "error.h"
#include "integer.h"

using namespace argon::vm::datatype;

#define CHECK_INTEGER(left, right)                                          \
    if (!(AR_TYPEOF(left, type_int_) || AR_TYPEOF(left, type_uint_)) ||     \
        !(AR_TYPEOF(right, type_int_) || AR_TYPEOF(right, type_uint_)))     \
        return nullptr

#define SIMPLE_OP(left, right, op)                                          \
    do {                                                                    \
        CHECK_INTEGER(left, right);                                         \
                                                                            \
        if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))    \
            return (ArObject *) UIntNew(left->uint op right->uint);         \
                                                                            \
        return (ArObject *) IntNew(left->sint op right->sint);              \
    } while(0)

ARGON_FUNCTION(number_parse, parse,
               "Convert a string or number to number, if possible.\n"
               "\n"
               "- Parameter obj: obj to convert.\n"
               "- Returns: integer number.\n",
               "sx: obj, i: base", false, false) {
    ArBuffer buffer{};

    const auto *self_type = (const TypeInfo *) ((Function *) _func)->base;
    auto base = (int) ((Integer *) args[1])->sint;

    if (AR_TYPEOF(*args, type_int_)) {
        if (self_type == type_int_)
            return IncRef(*args);

        return (ArObject *) UIntNew(((Integer *) *args)->sint);
    } else if (AR_TYPEOF(*args, type_uint_)) {
        if (self_type == type_uint_)
            return IncRef(*args);

        return (ArObject *) IntNew((IntegerUnderlying) ((Integer *) *args)->uint);
    }

    if (!BufferGet(*args, &buffer, argon::vm::datatype::BufferFlags::READ))
        return nullptr;

    if (self_type == type_int_) {
        auto *result = IntNew((const char *) buffer.buffer, base);

        BufferRelease(&buffer);

        return (ArObject *) result;
    }

    auto *result = UIntNew((const char *) buffer.buffer, base);

    BufferRelease(&buffer);

    return (ArObject *) result;
}

ARGON_METHOD(number_bits, bits,
             "Return number of bits necessary to represent an integer in binary.\n"
             "\n"
             "- Returns: Number of bits.\n",
             nullptr, false, false) {
    if (AR_TYPEOF(_self, type_int_))
        return (ArObject *) IntNew(IntegerCountBits(((Integer *) _self)->sint));

    return (ArObject *) UIntNew(IntegerCountBits(((Integer *) _self)->uint));
}

ARGON_METHOD(number_digits, digits,
             "Return number of digits necessary to represent an integer in the given numeric base.\n"
             "\n"
             "- Parameter base: Numeric base (2, 8, 10, 16).\n"
             "- Returns: Number of digits.\n",
             "iu: base", false, false) {
    const auto *r_base = (Integer *) args[0];
    int base;

    if (AR_TYPEOF(r_base, type_int_) && r_base->sint < 0) {
        ErrorFormat(kValueError[0], "numeric base cannot be negative");
        return nullptr;
    } else if (r_base->uint > 26) {
        ErrorFormat(kValueError[0], "numeric base cannot be greater than 26");
        return nullptr;
    }

    base = (int) r_base->uint;

    if (AR_TYPEOF(_self, type_int_))
        return (ArObject *) IntNew(IntegerCountDigits(((Integer *) _self)->sint, base));

    return (ArObject *) UIntNew(IntegerCountDigits(((Integer *) _self)->uint, base));
}

const FunctionDef number_methods[] = {
        number_parse,

        number_bits,
        number_digits,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots number_objslot = {
        number_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *number_compare(const Integer *self, const Integer *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    auto left = self->uint;
    auto right = other->uint;

    ARGON_RICH_COMPARE_CASES(left, right, mode)
}

ArSize number_hash(const Integer *self) {
    return self->uint;
}

ArObject *integer_add(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, +);
}

ArObject *integer_sub(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, -);
}

ArObject *integer_mul(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, *);
}

ArObject *integer_div(const Integer *left, const Integer *right) {
    CHECK_INTEGER(left, right);

    if (right->uint == 0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))
        return (ArObject *) UIntNew(left->uint / right->uint);

    return (ArObject *) IntNew(left->sint / right->sint);
}

ArObject *integer_mod(const Integer *left, const Integer *right) {
    IntegerUnderlying ans;

    CHECK_INTEGER(left, right);

    if (right->uint == 0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))
        return (ArObject *) UIntNew(left->uint % right->uint);

    ans = left->sint % right->sint;
    if (ans < 0)
        ans += right->sint;

    return (ArObject *) IntNew(ans);
}

ArObject *integer_pos(Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) IncRef(self);

    if (self->sint < 0)
        return (ArObject *) IntNew(-self->sint);

    return (ArObject *) IncRef(self);
}

ArObject *integer_neg(Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) IncRef(self);

    return (ArObject *) IntNew(-self->sint);
}

ArObject *integer_land(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, &);
}

ArObject *integer_lor(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, |);
}

ArObject *integer_lxor(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, ^);
}

ArObject *integer_shl(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, <<);
}

ArObject *integer_shr(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, >>);
}

ArObject *integer_inv(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(~self->uint);

    return (ArObject *) IntNew(~self->sint);
}

ArObject *integer_inc(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(self->uint + 1);

    return (ArObject *) IntNew(self->sint + 1);
}

ArObject *integer_dec(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(self->uint - 1);

    return (ArObject *) IntNew(self->sint - 1);
}

const OpSlots integer_ops = {
        (BinaryOp) integer_add,
        (BinaryOp) integer_sub,
        (BinaryOp) integer_mul,
        (BinaryOp) integer_div,
        (BinaryOp) integer_div,
        (BinaryOp) integer_mod,
        (UnaryOp) integer_pos,
        (UnaryOp) integer_neg,
        (BinaryOp) integer_land,
        (BinaryOp) integer_lor,
        (BinaryOp) integer_lxor,
        (BinaryOp) integer_shl,
        (BinaryOp) integer_shr,
        (UnaryOp) integer_inv,
        (BinaryOp) integer_add,
        (BinaryOp) integer_sub,
        (UnaryOp) integer_inc,
        (UnaryOp) integer_dec
};

ArObject *integer_repr(const Integer *self) {
    return (ArObject *) StringFormat("%i", self->sint);
}

bool integer_is_true(const Integer *self) {
    return self->sint > 0;
}

TypeInfo IntegerType = {
        AROBJ_HEAD_INIT_TYPE,
        "Int",
        nullptr,
        nullptr,
        sizeof(Integer),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) number_hash,
        (Bool_UnaryOp) integer_is_true,
        (CompareOp) number_compare,
        (UnaryConstOp) integer_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &number_objslot,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_int_ = &IntegerType;

ArObject *uinteger_repr(const Integer *self) {
    return (ArObject *) StringFormat("%u", self->uint);
}

bool uinteger_is_true(const Integer *self) {
    return self->uint > 0;
}

TypeInfo UIntegerType = {
        AROBJ_HEAD_INIT_TYPE,
        "UInt",
        nullptr,
        nullptr,
        sizeof(Integer),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) number_hash,
        (Bool_UnaryOp) uinteger_is_true,
        (CompareOp) number_compare,
        (UnaryConstOp) uinteger_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &number_objslot,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_uint_ = &UIntegerType;

Integer *argon::vm::datatype::IntNew(IntegerUnderlying number) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr) {
        si->uint = 0;
        si->sint = number;
    }

    return si;
}

Integer *argon::vm::datatype::IntNew(const char *string, int base) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr) {
        si->uint = 0;
        si->sint = std::strtol(string, nullptr, base);
    }

    return si;
}

Integer *argon::vm::datatype::UIntNew(UIntegerUnderlying number) {
    auto *ui = MakeObject<Integer>(&UIntegerType);

    if (ui != nullptr)
        ui->uint = number;

    return ui;
}

Integer *argon::vm::datatype::UIntNew(const char *string, int base) {
    auto *ui = MakeObject<Integer>(&UIntegerType);

    if (ui != nullptr)
        ui->uint = std::strtoul(string, nullptr, base);

    return ui;
}
