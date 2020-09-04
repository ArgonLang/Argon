// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_DECIMAL_H_
#define ARGON_OBJECT_DECIMAL_H_

#include <string>
#include <object/arobject.h>

namespace argon::object {

    using DecimalUnderlayer = long double;

    struct Decimal : public ArObject {
        DecimalUnderlayer decimal;
    };

    Decimal *DecimalNew(DecimalUnderlayer number);

    Decimal *DecimalNewFromString(const std::string &string);

    extern const TypeInfo type_decimal_;

} // namespace argon::object

#endif // !ARGON_OBJECT_DECIMAL_H_
