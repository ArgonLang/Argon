// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AR_ROUTINE_H_
#define ARGON_VM_AR_ROUTINE_H_

#include <object/arobject.h>
#include "frame.h"

#define ARGON_VM_QUEUE_MAX_ROUTINES 255

namespace argon::vm {

    enum class ArRoutineStatus : unsigned char {
        RUNNING,
        RUNNABLE,
        BLOCKED
    };

    struct ArRoutine {
        ArRoutine *next;

        Frame *frame;

        ArRoutineStatus status;
    };

    ArRoutine *RoutineNew(Frame *frame, ArRoutineStatus status);

    void RoutineDel(ArRoutine *routine);

    inline ArRoutine *RoutineNew(Frame *frame) { return RoutineNew(frame, ArRoutineStatus::RUNNABLE); }

} // namespace argon::vm

#endif // !ARGON_VM_AR_ROUTINE_H_
