// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "boolean.h"

constexpr int kFalseAsInt = 0;
constexpr int kTrueAsInt = 1;

using namespace argon::vm::datatype;

bool boolean_is_true(const Boolean *self) {
    return self->value;
}

ArSize boolean_hash(const Boolean *self) {
    return self->value ? kTrueAsInt : kFalseAsInt;
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
const TypeInfo *argon::vm::datatype::type_boolean_ = &BooleanType;

#define BOOLEAN_TYPE(sname, export_name, value)                     \
constexpr Boolean sname {AROBJ_HEAD_INIT(&BooleanType), (value)};   \
Boolean *(export_name) = (Boolean*) &sname

BOOLEAN_TYPE(BoolTrue, argon::vm::datatype::True, true);
BOOLEAN_TYPE(BoolFalse, argon::vm::datatype::False, false);
