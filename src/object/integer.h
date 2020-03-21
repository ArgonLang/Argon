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
    public:
        explicit Integer(long number) : integer_(number) {}

        explicit Integer(const std::string &number, int base);
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_INTEGER_H_
