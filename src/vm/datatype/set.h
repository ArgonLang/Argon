// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_SET_H_
#define ARGON_VM_DATATYPE_SET_H_

#include "arobject.h"

namespace argon::vm::datatype {
    struct Set {
        AROBJ_HEAD;

    };
    extern const TypeInfo *type_set_;

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_SET_H_
