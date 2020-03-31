// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "decimal.h"

using namespace argon::object;

Decimal::Decimal(long double number) : Number(&type_decimal_), decimal_(number) {}

Decimal::Decimal(const std::string &number) : Number(&type_decimal_) {
    std::size_t idx;
    this->decimal_ = std::stold(number, &idx);
}
