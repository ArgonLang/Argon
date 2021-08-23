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
        BLOCKED,
        SUSPENDED
    };

    struct Panic {
        /* Prev Panic */
        Panic *panic;

        /* Pointer to panic object */
        argon::object::ArObject *object;

        /* This panic was recovered? */
        bool recovered;

        /* This panic was aborted? if so, a new panic has occurred during the management of this panic */
        bool aborted;
    };

    struct Defer {
        /* Prev Defer */
        Defer *defer;

        /* Pointer to frame that has generated this defer */
        Frame *frame;

        /* Pointer to function object */
        argon::object::ArObject *function;
    };

    struct ArRoutine {
        /* Next ArRoutine (used by ArRoutineQueue) */
        ArRoutine *next;

        /* Current execution frame */
        Frame *frame;

        /* Pointer to head of deferred stack */
        Defer *defer;

        /* Pointer to current executed defer */
        Defer *cu_defer;

        /* Pointer to object that describe actual routine panic (if any...) */
        Panic *panic;

        /* Store object references in a function that which can become recursive (e.g. list_str, map_str...) */
        argon::object::List *references;

        /* Context in which this routine was created */
        Context *context;

        /* Current recursion depth */
        argon::object::ArSize recursion_depth;

        /* Used by NotifyQueue to manage the queue */
        argon::object::ArSize ticket;

        /* Routine status */
        ArRoutineStatus status;
    };

    ArRoutine *RoutineNew(ArRoutineStatus status);

    ArRoutine *RoutineNew(Frame *frame, ArRoutineStatus status);

    inline ArRoutine *RoutineNew(Frame *frame) { return RoutineNew(frame, ArRoutineStatus::RUNNABLE); }

    ArRoutine *RoutineNew(Frame *frame, ArRoutine *routine, ArRoutineStatus status);

    inline ArRoutine *RoutineNew(Frame *frame, ArRoutine *routine) { return RoutineNew(frame, routine, ArRoutineStatus::RUNNABLE); }

    argon::object::ArObject *RoutineRecover(ArRoutine *routine);

    argon::object::ArObject *RoutineReturnGet(ArRoutine *routine);

    void RoutineReturnSet(ArRoutine *routine, argon::object::ArObject *object);

    void RoutineReset(ArRoutine *routine, ArRoutineStatus status);

    void RoutineDel(ArRoutine *routine);

    void RoutineNewDefer(ArRoutine *routine, argon::object::ArObject *func);

    void RoutinePopDefer(ArRoutine *routine);

    void RoutineNewPanic(ArRoutine *routine, argon::object::ArObject *object);

    void RoutinePopPanic(ArRoutine *routine);

    void RoutinePopPanics(ArRoutine *routine);

    inline bool RoutineIsPanicking(ArRoutine *routine) { return routine != nullptr && routine->panic != nullptr; }

} // namespace argon::vm

#endif // !ARGON_VM_AR_ROUTINE_H_
