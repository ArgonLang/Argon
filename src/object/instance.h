// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_INSTANCE_H_
#define ARGON_OBJECT_INSTANCE_H_

#include "object.h"
#include "struct.h"

namespace argon::object {

    struct Instance : ArObject {
        Struct *base;
        Namespace *properties;
    };

    extern const TypeInfo type_instance_;

    Instance *InstanceNew(Struct *base, Namespace *properties);

} // namespace argon::object

#endif // !ARGON_OBJECT_INSTANCE_H_
