// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MODULE_H_
#define ARGON_OBJECT_MODULE_H_

#include <object/objmgmt.h>
#include "namespace.h"
#include "string.h"

namespace argon::object {
    struct Module : ArObject {
        Namespace *module_ns;
        String *name;
    };

    Module *ModuleNew(const std::string &name);

} // namespace argon::object

#endif // !ARGON_OBJECT_MODULE_H_
