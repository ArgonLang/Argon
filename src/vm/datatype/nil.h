// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_NIL_H_
#define ARGON_VM_DATATYPE_NIL_H_

#include "arobject.h"

namespace argon::vm::datatype {
    struct NilBase {
        AROBJ_HEAD;

        bool value;
    };
    extern const TypeInfo *type_nil_;

    extern NilBase *Nil;
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_NIL_H_
