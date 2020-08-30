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
        routine->status = status;
    }

    return routine;
}

void argon::vm::RoutineDel(ArRoutine *routine) {
    FrameDel(routine->frame);
    Free(routine);
}

void argon::vm::RoutineCleanPanic(ArRoutine *routine) {
    argon::object::Release(routine->panic_object);
    routine->panic_object = nullptr;
}