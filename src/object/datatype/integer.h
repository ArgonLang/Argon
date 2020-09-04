// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_INTEGER_H_
#define ARGON_OBJECT_INTEGER_H_

#include <string>

#include <object/arobject.h>

namespace argon::object {

    using IntegerUnderlayer = long;

    struct Integer : ArObject {
        IntegerUnderlayer integer;
    };

    extern const TypeInfo type_integer_;

    Integer *IntegerNew(long number);

    Integer *IntegerNewFromString(const std::string &string, int base);

    int IntegerCountBits(Integer *number);

} // namespace argon::object

#endif // !ARGON_OBJECT_INTEGER_H_
