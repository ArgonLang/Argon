// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AREVAL_H_
#define ARGON_VM_AREVAL_H_

#include "ar_routine.h"

namespace argon::vm {
    void Eval(ArRoutine *routine);

    void Eval(ArRoutine *routine, Frame *frame);
} // namespace argon::vm

#endif // !ARGON_VM_AREVAL_H_
