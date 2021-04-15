// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRUCT_H_
#define ARGON_OBJECT_STRUCT_H_

#include <object/arobject.h>
#include "string.h"
#include "namespace.h"
#include "list.h"

namespace argon::object {
    struct Struct : ArObject {
        Namespace *names;
    };

    extern const TypeInfo type_struct_;

    Struct *StructNew(TypeInfo *type, ArObject **values, ArSize count);

} // namespace argon::object

#endif // !ARGON_OBJECT_STRUCT_H_
