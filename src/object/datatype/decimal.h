// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_DECIMAL_H_
#define ARGON_OBJECT_DECIMAL_H_

#include <float.h>

#include <string>
#include <object/arobject.h>

#include "integer.h"

namespace argon::object {

    using DecimalUnderlying = long double;

    struct Decimal : public ArObject {
        DecimalUnderlying decimal;
    };

    extern const TypeInfo type_decimal_;

    Decimal *DecimalNew(DecimalUnderlying number);

    Decimal *DecimalNewFromString(const std::string &string);

    unsigned long DecimalModf(DecimalUnderlying value, unsigned long *frac, int precision);

    unsigned long DecimalFrexp10(DecimalUnderlying value, unsigned long *frac, long *exp, int precision);

} // namespace argon::object

#endif // !ARGON_OBJECT_DECIMAL_H_
