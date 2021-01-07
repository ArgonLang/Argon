// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_SET_H_
#define ARGON_OBJECT_SET_H_

#include <object/arobject.h>

#include "hmap.h"

namespace argon::object {
    struct Set : ArObject {
        HMap set;
    };

    extern const TypeInfo type_set_;

    Set *SetNew();

    bool SetAdd(Set *set, ArObject *value);

} // namespace argon::object

#endif // !ARGON_OBJECT_SET_H_
