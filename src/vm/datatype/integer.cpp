// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "integer.h"

using namespace argon::vm::datatype;

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
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_uint_ = &UIntegerType;

Integer *argon::vm::datatype::IntNew(IntegerUnderlying number) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr)
        si->sint = number;

    return nullptr;
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

    return nullptr;
}

Integer *argon::vm::datatype::UIntNew(const char *string, int base) {
    auto *ui = MakeObject<Integer>(&IntegerType);

    if (ui != nullptr)
        ui->uint = std::strtoul(string, nullptr, base);

    return nullptr;
}