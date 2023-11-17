// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_PCHECK_H_
#define ARGON_VM_DATATYPE_PCHECK_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/dict.h>

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
    _ARGONAPI bool KParamLookup(Dict *kwargs, const char *key, const TypeInfo *type, ArObject **out, ArObject *_default, bool nil_as_default);

    _ARGONAPI bool KParamLookupBool(Dict *kwargs, const char *key, bool *out, bool _default);

    _ARGONAPI bool KParamLookupInt(Dict *kwargs, const char *key, IntegerUnderlying *out, IntegerUnderlying _default);

    _ARGONAPI bool KParamLookupStr(Dict *kwargs, const char *key, String **out, const char *_default, bool *out_isdef);

    _ARGONAPI bool KParamLookupUInt(Dict *kwargs, const char *key, UIntegerUnderlying *out, UIntegerUnderlying _default);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_PCHECK_H_
