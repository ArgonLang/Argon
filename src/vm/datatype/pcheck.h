// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_PCHECK_H_
#define ARGON_VM_DATATYPE_PCHECK_H_

#include "arobject.h"

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

    extern const TypeInfo *type_pcheck_;

    PCheck *PCheckNew(const char *description);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_PCHECK_H_
