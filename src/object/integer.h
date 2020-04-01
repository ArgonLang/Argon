// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_INTEGER_H_
#define ARGON_OBJECT_INTEGER_H_

#include <string>

#include "number.h"

namespace argon::object {
    class Integer : public Number {
        long integer_;

        friend long ToCInt(Integer *integer);

    public:

        explicit Integer(long number);

        explicit Integer(const std::string &number, int base);

        bool EqualTo(const Object *other) override;

        size_t Hash() override;
    };

    inline const TypeInfo type_integer_ = {
            .name=(const unsigned char *) "integer",
            .size=sizeof(Integer)
    };

    inline long ToCInt(Integer *integer) { return integer->integer_; }

} // namespace argon::object

#endif // !ARGON_OBJECT_INTEGER_H_
