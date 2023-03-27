// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AREVAL_H_
#define ARGON_VM_AREVAL_H_

#include <argon/vm/datatype/arobject.h>

#include <argon/vm/fiber.h>

namespace argon::vm {
    datatype::ArObject *Eval(Fiber *fiber);
}

#endif // !ARGON_VM_AREVAL_H_
