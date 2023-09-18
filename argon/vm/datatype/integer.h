// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_INTEGER_H_
#define ARGON_VM_DATATYPE_INTEGER_H_

#include <cfloat>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/decimal.h>

namespace argon::vm::datatype {
    using IntegerUnderlying = long long;
    using UIntegerUnderlying = unsigned long long;

    struct Integer {
        AROBJ_HEAD;

        union {
            IntegerUnderlying sint;
            UIntegerUnderlying uint;
        };
    };
    extern const TypeInfo *type_int_;
    extern const TypeInfo *type_uint_;

    inline bool IsIntType(const ArObject *object) {
        return AR_TYPEOF(object, type_int_) || AR_TYPEOF(object, type_uint_);
    }

    template<typename T>
    int IntegerCountBits(T number) {
        int count = 0;

        if (number < 0)
            number *= -1;

        while (number) {
            count++;
            number >>= 1u;
        }

        return count;
    }

    template<typename T>
    int IntegerCountDigits(T number, int base) {
        int count = 0;

        if (number == 0)
            return 1;

        while (number) {
            count++;
            number /= base;
        }

        return count;
    }

    template<typename T>
    DecimalUnderlying Integer2ScaledDouble(T num, int bits, int *exp) {
        int sign = 1;

        if (num < 0) {
            sign = -1;
            num = -num;
        }

        auto exceeded = bits;
        while (exceeded > LDBL_MANT_DIG) {
            num >>= 1;
            exceeded--;
        }

        *exp = bits - exceeded;

        assert(num > 0.0);
        return num * sign;
    }

    Integer *IntNew(IntegerUnderlying number);

    Integer *IntNew(const char *string, int base);

    Integer *UIntNew(UIntegerUnderlying number);

    Integer *UIntNew(const char *string, int base);
}

#endif // !ARGON_VM_DATATYPE_INTEGER_H_