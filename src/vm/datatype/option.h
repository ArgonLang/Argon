// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_OPTION_H_
#define ARGON_VM_DATATYPE_OPTION_H_

#include "arobject.h"

namespace argon::vm::datatype {
    struct Option {
        AROBJ_HEAD;

        ArObject *some;
    };
    extern const TypeInfo *type_option_;

    Option *OptionNew(ArObject *value);

    inline Option *OptionNew() {
        return OptionNew(nullptr);
    }
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_OPTION_H_