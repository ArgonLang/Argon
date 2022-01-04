// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/arobject.h>
#include <object/datatype/nil.h>
#include "frame.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

Frame *argon::vm::FrameNew(Code *code, Namespace *globals, Namespace *proxy_globals) {
    ArSize slots = code->stack_sz;
    Frame *frame;

    if (code->locals != nullptr)
        slots += code->locals->len;

    frame = (Frame *) Alloc(sizeof(Frame) + (slots * sizeof(void *)));

    if (frame != nullptr) {
        frame->back = nullptr;
        frame->globals = IncRef(globals);
        frame->proxy_globals = IncRef(proxy_globals);
        frame->instance = nullptr;
        frame->code = IncRef(code);
        frame->instr_ptr = (unsigned char *) code->instr;
        frame->eval_stack = frame->stack_extra_base;
        frame->locals = frame->stack_extra_base + code->stack_sz;
        frame->enclosed = nullptr;
        frame->return_value = nullptr;

        // Set locals slots to nullptr
        slots -= code->stack_sz;
        while (slots-- > 0)
            frame->locals[slots] = nullptr;
    }

    return frame;
}

void argon::vm::FrameDel(Frame *frame) {
    Code *code = frame->code;

    if (code->locals != nullptr) {
        for (ArSize i = 0; i < code->locals->len; i++)
            Release(frame->locals[i]);
    }

    // Do not release frame->instance, because it's point to frame->locals[0]
    Release(code);
    Release(frame->globals);
    Release(frame->proxy_globals);
    Release(frame->enclosed);
    Release(frame->return_value);

    Free(frame);
}

void argon::vm::FrameFillForCall(Frame *frame, Function *callable, ArObject **argv, ArSize argc) {
    ArSize local_idx = 0;

    // Push currying args
    if (callable->currying != nullptr) {
        for (ArSize i = 0; i < callable->currying->len; i++)
            frame->locals[local_idx++] = ListGetItem(callable->currying, i);
    }

    // Fill with stack args
    for (ArSize i = 0; i < argc; i++)
        frame->locals[local_idx++] = IncRef(argv[i]);

    // If method, set frame->instance
    if (callable->IsMethod())
        frame->instance = frame->locals[0];

    // If last parameter in variadic function is empty, fill it with NilVal
    if (callable->IsVariadic() && local_idx < callable->arity + 1)
        frame->locals[local_idx++] = NilVal;

    frame->enclosed = IncRef(callable->enclosed);
}