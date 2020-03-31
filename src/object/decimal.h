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
        explicit Decimal(long double number);

        explicit Decimal(const std::string &number);

        bool EqualTo(const Object *other) override;

        size_t Hash() override;
    };

    inline const TypeInfo type_decimal_ = {
            .name=(const unsigned char *) "decimal",
            .size=sizeof(Decimal)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_DECIMAL_H_
