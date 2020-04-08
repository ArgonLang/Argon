// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AR_ROUTINE_H_
#define ARGON_VM_AR_ROUTINE_H_

#include "frame.h"

namespace argon::vm {
    struct ArRoutine {
        Frame *frame;
    };
} // namespace argon::vm

#endif // !ARGON_VM_AR_ROUTINE_H_
