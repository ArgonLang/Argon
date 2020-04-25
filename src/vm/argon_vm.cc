// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/map.h>
#include "argon_vm.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

ArgonVM::ArgonVM() {
    this->main = ModuleNew("main");
}

ArObject *ArgonVM::EvalCode(Code *code) {
    // TODO: STUB
    Frame *frame = FrameNew(code);
    ArRoutine *routine = (ArRoutine *) Alloc(sizeof(ArRoutine));

    routine->frame = frame;

    frame->globals = this->main->module_ns;

    this->Eval(routine);

    Free(routine);

    FrameDel(frame);

    return nullptr;
}