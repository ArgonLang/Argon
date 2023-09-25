// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_RESULT_H_
#define ARGON_VM_DATATYPE_RESULT_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    struct Result {
        AROBJ_HEAD;

        ArObject *value;

        bool success;
    };
    _ARGONAPI extern const TypeInfo *type_result_;

    Result *ResultNew(ArObject *value, bool success);
}

#endif // !ARGON_VM_DATATYPE_RESULT_H_