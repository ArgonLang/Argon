// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "frame.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

Frame *argon::vm::FrameNew(Code *code) {
    auto frame = (Frame *) Alloc(sizeof(Frame) + (code->stack_sz * sizeof(object::ArObject *)) +
                                 (code->locals->len * sizeof(object::ArObject *)));

    assert(frame != nullptr); // TODO: NOMEM

    IncRef(code);

    frame->back = nullptr;
    frame->code = code;
    frame->instr_ptr = (unsigned char *) code->instr;
    frame->eval_stack = (object::ArObject **) frame->stack_extra_base;
    frame->locals = (ArObject **) (frame->stack_extra_base + (code->stack_sz * sizeof(object::ArObject *)));

    return frame;
}

void argon::vm::FrameDel(Frame *frame) {
    Release(frame->code);
    Free(frame);
}
