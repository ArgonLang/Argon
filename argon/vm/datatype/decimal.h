// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_DECIMAL_H_
#define ARGON_VM_DATATYPE_DECIMAL_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    using DecimalUnderlying = long double;

    struct Decimal {
        AROBJ_HEAD;

        DecimalUnderlying decimal;
    };
    _ARGONAPI extern const TypeInfo *type_decimal_;

    Decimal *DecimalNew(DecimalUnderlying number);

    Decimal *DecimalNew(const char *string);
}

#endif // !ARGON_VM_DATATYPE_DECIMAL_H_
