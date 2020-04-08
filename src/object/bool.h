// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BOOL_H_
#define ARGON_OBJECT_BOOL_H_

#include <new>
#include <memory/memory.h>

#include "object.h"

namespace argon::object {
    struct Bool : public ArObject {
        bool value;
    };

    extern Bool *True;
    extern Bool *False;

} // namespace argon::object

#endif // !ARGON_OBJECT_BOOL_H_

