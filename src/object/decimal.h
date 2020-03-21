// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_DECIMAL_H_
#define ARGON_OBJECT_DECIMAL_H_

#include <string>

#include "number.h"

namespace argon::object {
    class Decimal : public Number {
        long double decimal_;
    public:
        explicit Decimal(long double number) : decimal_(number) {}

        explicit Decimal(const std::string &number);
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_DECIMAL_H_
