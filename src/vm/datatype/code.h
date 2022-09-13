// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_CODE_H_
#define ARGON_VM_DATATYPE_CODE_H_

#include "arobject.h"

namespace argon::vm::datatype {
    struct Code {
        AROBJ_HEAD;
    };
    extern const TypeInfo *type_code_;
}

#endif // !ARGON_VM_DATATYPE_CODE_H_
