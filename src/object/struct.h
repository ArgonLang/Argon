// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRUCT_H_
#define ARGON_OBJECT_STRUCT_H_

#include "object.h"
#include "string.h"
#include "namespace.h"
#include "list.h"

namespace argon::object {
    struct Struct : ArObject {
        String *name;
        Namespace *names;
        List *impls;

        unsigned short properties_count;
    };

    extern const TypeInfo type_struct_;

    Struct *StructNew(String *name, Namespace *names, List *mro);

} // namespace argon::object

#endif // !ARGON_OBJECT_STRUCT_H_
