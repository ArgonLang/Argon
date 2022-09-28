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

const TypeInfo IntegerType = {
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
        nullptr,
        (CompareOp) number_compare,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_int_ = &IntegerType;

const TypeInfo UIntegerType = {
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
        nullptr,
        (CompareOp) number_compare,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_uint_ = &UIntegerType;

Integer *argon::vm::datatype::IntNew(IntegerUnderlying number) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr)
        si->sint = number;

    return si;
}

Integer *argon::vm::datatype::IntNew(const char *string, int base) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr)
        si->sint = std::strtol(string, nullptr, base);

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