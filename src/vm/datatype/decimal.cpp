// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "decimal.h"

using namespace argon::vm::datatype;

TypeInfo DecimalType = {
        AROBJ_HEAD_INIT_TYPE,
        "Decimal",
        nullptr,
        nullptr,
        sizeof(Decimal),
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
const TypeInfo *argon::vm::datatype::type_decimal_ = &DecimalType;

Decimal *argon::vm::datatype::DecimalNew(DecimalUnderlying number) {
    auto *decimal = MakeObject<Decimal>(type_decimal_);

    if (decimal != nullptr)
        decimal->decimal = number;

    return decimal;
}

Decimal *argon::vm::datatype::DecimalNew(const char *string) {
    auto *decimal = MakeObject<Decimal>(type_decimal_);

    if (decimal != nullptr)
        decimal->decimal = std::strtod(string, nullptr);

    return decimal;
}