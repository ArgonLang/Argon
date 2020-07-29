// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/map.h>
#include "argon_vm.h"
#include "areval.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

ArgonVM::ArgonVM() {
    this->main = ModuleNew("main");
}

ArObject *ArgonVM::EvalCode(Code *code) {
    // TODO: STUB
    ArRoutine *routine = (ArRoutine *) Alloc(sizeof(ArRoutine));
    Frame *frame = FrameNew(code, this->main->module_ns, nullptr);

    Eval(routine, frame);

    FrameDel(frame);
    Free(routine);

    return nullptr;
}