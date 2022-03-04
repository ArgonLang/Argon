// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AREVAL_H_
#define ARGON_VM_AREVAL_H_

#include <object/arobject.h>
#include <object/datatype/frame.h>

#include "arroutine.h"

namespace argon::vm {
    argon::object::ArObject *Eval(ArRoutine *routine);

    argon::object::ArObject *Eval(ArRoutine *routine, argon::object::Frame *frame);
} // namespace argon::vm

#endif // !ARGON_VM_AREVAL_H_
