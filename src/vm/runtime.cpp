// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/datatype/setup.h>

#include "runtime.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

ArObject *argon::vm::GetLastError() {
    // TODO: STUB
    assert(false);
}

bool argon::vm::Initialize() {
    // TODO: STUB
    assert(memory::MemoryInit());
    assert(Setup());
    return true;
}

bool argon::vm::IsPanicking() {
    // TODO: STUB
    return false;
}

void argon::vm::Panic(datatype::ArObject *panic) {
    // TODO: STUB
    assert(false);
}
