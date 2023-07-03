// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/panic.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

Panic *argon::vm::PanicNew(Panic *prev, Frame *frame, ArObject *object) {
    auto *panic = (Panic *) memory::Alloc(sizeof(Panic));

    PanicFill(panic, prev, frame, object);

    return panic;
}

void argon::vm::PanicFill(Panic *panic, Panic *prev, Frame *frame, ArObject *object) {
    if (panic == nullptr)
        return;

    panic->panic = prev;
    panic->frame = frame;
    panic->object = IncRef(object);
    panic->recovered = false;
    panic->aborted = prev != nullptr;

    if (panic->frame != nullptr)
        panic->frame->counter++;
}
