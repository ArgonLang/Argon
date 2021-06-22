// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRUCT_H_
#define ARGON_OBJECT_STRUCT_H_

#include <object/arobject.h>

#include "namespace.h"

namespace argon::object {
    struct Struct : ArObject {
        Namespace *names;
    };

    extern const TypeInfo *type_struct_;

    struct NativeWrapper : ArObject {
        const char *name;
        int offset;
        NativeMemberType mtype;
        bool readonly;
    };

    extern const TypeInfo *type_native_wrapper_;

    NativeWrapper *NativeWrapperNew(const NativeMember *member);

    Struct *StructNewPositional(TypeInfo *type, ArObject **values, ArSize count);

    Struct *StructNewKeyPair(TypeInfo *type, ArObject **values, ArSize count);

} // namespace argon::object

#endif // !ARGON_OBJECT_STRUCT_H_
