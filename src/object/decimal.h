// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_DECIMAL_H_
#define ARGON_OBJECT_DECIMAL_H_

#include <string>

#include "number.h"

namespace argon::object {
    struct Decimal : public ArObject {
        long double decimal;
    };

    Decimal *DecimalNew(long double number);

    Decimal *DecimalNewFromString(const std::string &string);

} // namespace argon::object

#endif // !ARGON_OBJECT_DECIMAL_H_
