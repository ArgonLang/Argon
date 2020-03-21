// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "decimal.h"

using namespace argon::object;

Decimal::Decimal(const std::string &number) {
    std::size_t idx;
    this->decimal_ = std::stold(number, &idx);
}
