// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/integer.h>

#include <argon/vm/datatype/boolean.h>

constexpr int kFalseAsInt = 0;
constexpr int kTrueAsInt = 1;

using namespace argon::vm::datatype;

ArObject *bool_land(const Boolean *left, const ArObject *right) {
    return BoolToArBool(left->value & IsTrue(right));
}

ArObject *bool_lor(const Boolean *left, const ArObject *right) {
    return BoolToArBool(left->value | IsTrue(right));
}

ArObject *bool_lxor(const Boolean *left, const ArObject *right) {
    return BoolToArBool(left->value ^ IsTrue(right));
}

const OpSlots bool_ops{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) bool_land,
        (BinaryOp) bool_lor,
        (BinaryOp) bool_lxor,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *boolean_compare(Boolean *self, ArObject *other, CompareMode mode) {
    IntegerUnderlying l = self->value;
    IntegerUnderlying r;

    if ((ArObject *) self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (AR_TYPEOF(other, type_boolean_))
        r = ((Boolean *) other)->value;
    else if (AR_TYPEOF(other, type_int_))
        r = ((Integer *) other)->sint;
    else if (AR_TYPEOF(other, type_uint_))
        r = ((Integer *) other)->uint > 0 ? kTrueAsInt : kFalseAsInt;
    else
        return nullptr;

    ARGON_RICH_COMPARE_CASES(l, r, mode);
}

ArObject *boolean_repr(const Boolean *self) {
    return (ArObject *) StringFormat("%s", self->value ? "true" : "false");
}

ArSize boolean_hash(const Boolean *self) {
    return self->value ? kTrueAsInt + 1 : kFalseAsInt + 1;
}

bool boolean_is_true(const Boolean *self) {
    return self->value;
}

TypeInfo BooleanType = {
        AROBJ_HEAD_INIT_TYPE,
        "Bool",
        nullptr,
        nullptr,
        sizeof(Boolean),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) boolean_hash,
        (Bool_UnaryOp) boolean_is_true,
        (CompareOp) boolean_compare,
        (UnaryConstOp) boolean_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &bool_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_boolean_ = &BooleanType;

#define BOOLEAN_TYPE(sname, export_name, value)                     \
constexpr Boolean sname {AROBJ_HEAD_INIT(&BooleanType), (value)};   \
Boolean *(export_name) = (Boolean*) &sname

BOOLEAN_TYPE(BoolTrue, argon::vm::datatype::True, true);
BOOLEAN_TYPE(BoolFalse, argon::vm::datatype::False, false);
