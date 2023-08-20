// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <climits>

#include <argon/vm/datatype/integer.h>

#include <argon/vm/mod/modules.h>

using namespace argon::vm::datatype;

const ModuleEntry limits_entries[] = {
        ARGON_MODULE_SENTINEL
};

bool LimitsInit(Module *self) {
#define LIMIT(name, op)                                                     \
    do {                                                                    \
        if((tmp = (ArObject*) op) == nullptr)                               \
            return false;                                                   \
                                                                            \
        if(!ModuleAddObject(self, #name, tmp, MODULE_ATTRIBUTE_DEFAULT)) {  \
            Release(tmp);                                                   \
            return false;                                                   \
        }                                                                   \
                                                                            \
        Release(tmp);                                                       \
    } while(0)

    ArObject *tmp;

    LIMIT(WORDSZ, IntNew(sizeof(void *)));

    // Decimal
    LIMIT(DECIMAL_EPSILON, DecimalNew(LDBL_EPSILON));
    LIMIT(DECIMAL_MANT_DIG, IntNew(LDBL_MANT_DIG));
    LIMIT(DECIMAL_MAX, DecimalNew(LDBL_MAX));
    LIMIT(DECIMAL_MAX_EXP, IntNew(LDBL_MAX_EXP));
    LIMIT(DECIMAL_MIN, DecimalNew(LDBL_MIN));
    LIMIT(DECIMAL_MIN_EXP, IntNew(LDBL_MIN_EXP));

    // Int
    LIMIT(INT_BITS, IntNew(sizeof(IntegerUnderlying) * 8));
    LIMIT(INT_MAX, IntNew(LONG_LONG_MAX));
    LIMIT(INT_MIN, IntNew(LONG_LONG_MIN));

    // UInt
    LIMIT(UINT_BITS, IntNew(sizeof(UIntegerUnderlying) * 8));
    LIMIT(UINT_MAX, UIntNew(ULONG_LONG_MAX));

    return true;
}

constexpr ModuleInit ModuleLimits = {
        "limits",
        "Defines constants with various limits for the specific system in use.",
        nullptr,
        limits_entries,
        LimitsInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_limits_ = &ModuleLimits;