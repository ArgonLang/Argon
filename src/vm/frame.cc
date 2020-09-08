// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/objmgmt.h>
#include "frame.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

Frame *argon::vm::FrameNew(object::Code *code, object::Namespace *globals, object::Namespace *proxy_globals) {
    auto frame = (Frame *) Alloc(sizeof(Frame) +
                                 (code->stack_sz * sizeof(object::ArObject *)) +
                                 (code->locals->len * sizeof(object::ArObject *)) +
                                 (code->enclosed->len * sizeof(object::ArObject *)));

    assert(frame != nullptr); // TODO: NOMEM

    IncRef(code);
    IncRef(globals);
    IncRef(proxy_globals);

    frame->back = nullptr;
    frame->globals = globals;
    frame->proxy_globals = proxy_globals;
    frame->code = code;
    frame->instr_ptr = (unsigned char *) code->instr;
    frame->eval_stack = (object::ArObject **) frame->stack_extra_base;
    frame->locals = (ArObject **) (frame->stack_extra_base + (code->stack_sz * sizeof(object::ArObject *)));
    frame->enclosed = (ArObject **) (frame->locals + (code->locals->len * sizeof(object::ArObject *)));

    return frame;
}

void argon::vm::FrameDel(Frame *frame) {
    Code *code = frame->code;

    if (code->locals != nullptr) {
        for (size_t i = 0; i < code->locals->len; i++)
            Release(frame->locals[i]);
    }

    if (code->enclosed != nullptr) {
        for (size_t i = 0; i < code->enclosed->len; i++)
            Release(frame->enclosed[i]);
    }

    Release(code);
    Release(frame->globals);
    Release(frame->proxy_globals);
    Free(frame);
}
