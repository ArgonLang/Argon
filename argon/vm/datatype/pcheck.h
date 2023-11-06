// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_PCHECK_H_
#define ARGON_VM_DATATYPE_PCHECK_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    struct Param {
        char *name;

        const TypeInfo *types[];
    };

    struct PCheck {
        AROBJ_HEAD;

        unsigned short count;

        Param **params;
    };

    _ARGONAPI extern const TypeInfo *type_pcheck_;

    PCheck *PCheckNew(const char *description);

    bool VariadicCheckPositional(const char *name, unsigned int nargs, unsigned int min, unsigned int max);

    // KWParameters utilities
    bool KParamLookupInt(Dict *kwargs, const char *key, IntegerUnderlying *out, IntegerUnderlying _default);

    //String *KParamLookupStr(Dict *kwargs, const char *key, const char *_default, bool *);

    bool KParamLookupStr(Dict *kwargs, const char *key, String **out, const char *_default, bool *out_isdef);

    bool KParamLookupUInt(Dict *kwargs, const char *key, UIntegerUnderlying *out, UIntegerUnderlying _default);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_PCHECK_H_
