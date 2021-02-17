// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/arobject.h>
#include "frame.h"

using namespace argon::vm;
using namespace argon::object;
using namespace argon::memory;

Frame *argon::vm::FrameNew(object::Code *code, object::Namespace *globals, object::Namespace *proxy_globals) {
    ArSize slots = code->stack_sz + code->locals->len + code->enclosed->len;
    auto *frame = (Frame *) Alloc(sizeof(Frame) + (slots * sizeof(void *)));

    if (frame != nullptr) {
        IncRef(code);
        IncRef(globals);
        IncRef(proxy_globals);

        frame->back = nullptr;
        frame->globals = globals;
        frame->proxy_globals = proxy_globals;
        frame->instance = nullptr;
        frame->code = code;
        frame->instr_ptr = (unsigned char *) code->instr;
        frame->eval_stack = frame->stack_extra_base;
        frame->locals = frame->stack_extra_base + code->stack_sz;
        frame->enclosed = frame->locals + code->locals->len;

        // Set locals/enclosed slots to nullptr
        slots -= code->stack_sz;
        while (slots-- > 0)
            frame->locals[slots] = nullptr;
    }

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
    // Do not release frame->instance, because it's point to frame->locals[0]
    Free(frame);
}
