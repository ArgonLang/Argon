// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_INTEGER_H_
#define ARGON_VM_DATATYPE_INTEGER_H_

#include "arobject.h"

namespace argon::vm::datatype {
    using IntegerUnderlying = long;
    using UIntegerUnderlying = unsigned long;

    struct Integer {
        AROBJ_HEAD;

        union {
            IntegerUnderlying sint;
            UIntegerUnderlying uint;
        };
    };
    extern const TypeInfo *type_int_;
    extern const TypeInfo *type_uint_;

    Integer *IntNew(IntegerUnderlying number);

    Integer *IntNew(const char *string, int base);

    Integer *UIntNew(UIntegerUnderlying number);

    Integer *UIntNew(const char *string, int base);
}

#endif // !ARGON_VM_DATATYPE_INTEGER_H_