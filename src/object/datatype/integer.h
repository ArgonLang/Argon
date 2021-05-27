// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_INTEGER_H_
#define ARGON_OBJECT_INTEGER_H_

#include <string>

#include <object/arobject.h>

namespace argon::object {

    using IntegerUnderlying = long;

    struct Integer : ArObject {
        IntegerUnderlying integer;
    };

    extern const TypeInfo *type_integer_;

    Integer *IntegerNew(IntegerUnderlying number);

    Integer *IntegerNewFromString(const std::string &string, int base);

    int IntegerCountBits(Integer *number);

    int IntegerCountDigits(IntegerUnderlying number, IntegerUnderlying base);

} // namespace argon::object

#endif // !ARGON_OBJECT_INTEGER_H_
