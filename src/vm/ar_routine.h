// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_AR_ROUTINE_H_
#define ARGON_VM_AR_ROUTINE_H_

#include <object/arobject.h>

#include "context.h"
#include "frame.h"

#define ARGON_VM_QUEUE_MAX_ROUTINES 255

namespace argon::vm {

    enum class ArRoutineStatus : unsigned char {
        RUNNING,
        RUNNABLE,
        BLOCKED
    };

    struct ArRoutine {
        /* Next ArRoutine (used by ArRoutineQueue) */
        ArRoutine *next;

        /* Current execution frame */
        Frame *frame;

        /* Pointer to object that describe actual routine panic (if any...) */
        argon::object::ArObject *panic_object;

        /* Context in which this routine was created */
        Context *context;

        /* Routine status */
        ArRoutineStatus status;
    };

    ArRoutine *RoutineNew(Frame *frame, ArRoutineStatus status);

    void RoutineDel(ArRoutine *routine);

    void RoutineCleanPanic(ArRoutine *routine);

    inline ArRoutine *RoutineNew(Frame *frame) { return RoutineNew(frame, ArRoutineStatus::RUNNABLE); }

} // namespace argon::vm

#endif // !ARGON_VM_AR_ROUTINE_H_
