// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/objmgmt.h>
#include "ar_routine.h"

using namespace argon::vm;
using namespace argon::memory;

ArRoutine *argon::vm::RoutineNew(Frame *frame, ArRoutineStatus status) {
    auto routine = (ArRoutine *) Alloc(sizeof(ArRoutine));

    if (routine != nullptr) {
        routine->next = nullptr;
        routine->frame = frame;
        routine->defer = nullptr;
        routine->cu_defer = nullptr;
        routine->status = status;
    }

    return routine;
}

void argon::vm::RoutineDel(ArRoutine *routine) {
    FrameDel(routine->frame);
    Free(routine);
}

void argon::vm::RoutineNewDefer(ArRoutine *routine, argon::object::ArObject *func) {
    auto defer = (Defer *) Alloc(sizeof(Defer));

    if (defer != nullptr) {
        defer->defer = routine->defer;
        routine->defer = defer;

        defer->frame = routine->frame;
        argon::object::IncRef(func);
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

    argon::object::Release(defer->function);
    argon::memory::Free(defer);
}

void argon::vm::RoutineNewPanic(ArRoutine *routine, argon::object::ArObject *object) {
    auto panic = (struct Panic *) Alloc(sizeof(struct Panic));

    if (panic != nullptr) {
        argon::object::IncRef(object);

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
        argon::object::Release(panic->object);
        routine->panic = panic->panic;
        argon::memory::Free(panic);
    }
}