// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/panic.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

Panic *argon::vm::PanicNew(Panic *prev, ArObject *object) {
    auto *panic = (Panic *) memory::Alloc(sizeof(Panic));

    if (panic != nullptr) {
        panic->panic = prev;
        panic->object = IncRef(object);
        panic->recovered = false;
        panic->aborted = prev != nullptr;
    }

    return panic;
}
