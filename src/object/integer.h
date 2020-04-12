// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_INTEGER_H_
#define ARGON_OBJECT_INTEGER_H_

#include <string>

#include "object.h"

namespace argon::object {
    struct Integer : ArObject {
        long integer;
    };

    extern const TypeInfo type_integer_;

    Integer *IntegerNew(long number);

    Integer *IntegerNewFromString(const std::string &string, int base);

} // namespace argon::object

#endif // !ARGON_OBJECT_INTEGER_H_
