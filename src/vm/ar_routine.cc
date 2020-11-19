// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/objmgmt.h>
#include "ar_routine.h"

using namespace argon::vm;
using namespace argon::memory;
using namespace argon::object;

ArRoutine *argon::vm::RoutineNew(ArRoutineStatus status) {
    auto routine = (ArRoutine *) Alloc(sizeof(ArRoutine));

    if (routine != nullptr) {
        routine->next = nullptr;
        routine->frame = nullptr;
        routine->defer = nullptr;
        routine->cu_defer = nullptr;
        routine->status = status;
    }

    return routine;
}

ArRoutine *argon::vm::RoutineNew(Frame *frame, ArRoutineStatus status) {
    auto routine = RoutineNew(status);

    if (routine != nullptr) {
        routine->next = nullptr;
        routine->frame = frame;
        routine->defer = nullptr;
        routine->cu_defer = nullptr;
    }

    return routine;
}

ArObject *argon::vm::RoutineRecover(ArRoutine *routine) {
    ArObject *err = nullptr;

    if (routine != nullptr) {
        if (routine->cu_defer != nullptr && routine->cu_defer->panic != nullptr) {
            err = routine->cu_defer->panic->object;
            IncRef(err);
            assert(routine->panic == routine->cu_defer->panic);
            routine->cu_defer->panic = nullptr;
            RoutinePopPanic(routine);
        }
    }

    return err;
}

void argon::vm::RoutineReset(ArRoutine *routine, ArRoutineStatus status) {
    if (routine != nullptr) {
        routine->next = nullptr;

        if (routine->frame != nullptr)
            FrameDel(routine->frame);
        routine->frame = nullptr;

        while (routine->panic != nullptr)
            RoutinePopPanic(routine);

        assert(routine->cu_defer == nullptr);

        routine->status = status;
    }
}

void argon::vm::RoutineDel(ArRoutine *routine) {
    RoutineReset(routine, ArRoutineStatus::RUNNABLE);
    Free(routine);
}

void argon::vm::RoutineNewDefer(ArRoutine *routine, ArObject *func) {
    auto defer = (Defer *) Alloc(sizeof(Defer));

    if (defer != nullptr) {
        defer->defer = routine->defer;
        routine->defer = defer;

        defer->frame = routine->frame;
        IncRef(func);
        defer->function = func;
        defer->panic = nullptr;
        return;
    }

    assert(false);
}

void argon::vm::RoutineDelDefer(ArRoutine *routine) {
    auto defer = routine->defer;

    routine->defer = defer->defer;

    if (routine->cu_defer == defer)
        routine->cu_defer = nullptr;

    Release(defer->function);
    argon::memory::Free(defer);
}

void argon::vm::RoutineNewPanic(ArRoutine *routine, ArObject *object) {
    auto panic = (struct Panic *) Alloc(sizeof(struct Panic));

    if (panic != nullptr) {
        IncRef(object);

        panic->panic = routine->panic;
        routine->panic = panic;

        panic->object = object;
        panic->recovered = false;
        panic->aborted = false;

        if (routine->cu_defer != nullptr) {
            routine->cu_defer->panic->aborted = true;
            routine->cu_defer->panic = nullptr;
        }
    }
}

void argon::vm::RoutinePopPanic(ArRoutine *routine) {
    auto panic = routine->panic;

    if (panic != nullptr) {
        Release(panic->object);
        routine->panic = panic->panic;
        argon::memory::Free(panic);
    }
}