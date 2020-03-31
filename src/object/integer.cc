// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "integer.h"

using namespace argon::object;

Integer::Integer(long number) : Number(&type_integer_), integer_(number) {}

Integer::Integer(const std::string &number, int base) : Number(&type_integer_) {
    this->integer_ = std::strtol(number.c_str(), nullptr, base);
}
