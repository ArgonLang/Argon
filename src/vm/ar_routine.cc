// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "ar_routine.h"

using namespace argon::vm;
using namespace argon::memory;

ArRoutine *argon::vm::RoutineNew(Frame *frame, RoutineStatus status) {
    auto routine = (ArRoutine *) Alloc(sizeof(ArRoutine));

    if (routine != nullptr) {
        routine->next = nullptr;
        routine->prev = nullptr;
        routine->frame = frame;
        routine->status = status;
    }

    return routine;
}
