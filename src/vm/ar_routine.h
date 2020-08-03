// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AR_ROUTINE_H_
#define ARGON_VM_AR_ROUTINE_H_

#include <object/arobject.h>
#include "frame.h"

namespace argon::vm {

    enum class RoutineStatus : unsigned char {
        RUNNING,
        RUNNABLE,
        BLOCKED
    };

    struct ArRoutine {
        ArRoutine *next;
        ArRoutine *prev;

        Frame *frame;

        RoutineStatus status;
    };

    ArRoutine *RoutineNew(Frame *frame, RoutineStatus status);

    inline ArRoutine *RoutineNew(Frame *frame) { return RoutineNew(frame, RoutineStatus::RUNNABLE); }

} // namespace argon::vm

#endif // !ARGON_VM_AR_ROUTINE_H_
