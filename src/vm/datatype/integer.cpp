// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "boolean.h"
#include "integer.h"

using namespace argon::vm::datatype;

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
        (ArSize_UnaryOp)number_hash,
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
        nullptr,
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
        (ArSize_UnaryOp)number_hash,
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
        nullptr,
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
    auto *ui = MakeObject<Integer>(&IntegerType);

    if (ui != nullptr)
        ui->uint = number;

    return ui;
}

Integer *argon::vm::datatype::UIntNew(const char *string, int base) {
    auto *ui = MakeObject<Integer>(&IntegerType);

    if (ui != nullptr)
        ui->uint = std::strtoul(string, nullptr, base);

    return ui;
}