// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "frame.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

Frame *argon::vm::FrameNew(Code *code) {
    auto frame = (Frame *) Alloc(sizeof(Frame) + (code->stack_sz * sizeof(object::Object *)) + code->locals->len);

    assert(frame != nullptr); // TODO: NOMEM

    IncStrongRef(code);

    frame->back = nullptr;
    frame->code = code;
    frame->instr_ptr = (unsigned char *) code->instr;
    frame->eval_stack = (object::Object **) frame->extras_;
    frame->eval_index = 0;

    return frame;
}

void argon::vm::FrameDel(Frame *frame) {
    ReleaseObject(frame->code);
    Free(frame);
}
