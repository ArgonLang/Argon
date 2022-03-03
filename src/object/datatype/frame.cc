// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/gc.h>
#include "error.h"
#include "frame.h"
#include "nil.h"

using namespace argon::object;

void frame_cleanup(Frame *self) {
    Code *code = self->code;

    self->lock.~Mutex();

    if (code->locals != nullptr) {
        for (ArSize i = 0; i < code->locals->len; i++)
            Release(self->locals[i]);
    }

    // Do not release frame->instance, because it's point to frame->locals[0]
    Release(code);
    Release(self->globals);
    Release(self->proxy_globals);
    Release(self->enclosed);
    Release(self->return_value);
}

void frame_trace(Frame *self, VoidUnaryOp trace) {
    trace(self->globals);
    trace(self->proxy_globals);
    trace(self->instance);
    trace(self->return_value);
    trace(self->enclosed);
}

bool frame_is_true(ArObject *self) {
    return true;
}

const TypeInfo FrameType = {
        TYPEINFO_STATIC_INIT,
        "frame",
        nullptr,
        sizeof(Frame),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) frame_cleanup,
        (Trace) frame_trace,
        nullptr,
        frame_is_true,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_frame_ = &FrameType;

Frame *argon::object::FrameNew(Code *code, Namespace *globals, Namespace *proxy) {
    ArSize slots = code->stack_sz;
    Frame *frame;

    if (code->locals != nullptr)
        slots += code->locals->len;

    if ((frame = (Frame *) GCNew(sizeof(Frame) + (slots * sizeof(void *)))) == nullptr)
        return (Frame *) argon::vm::Panic(error_out_of_memory);

    if (frame != nullptr) {
        frame->ref_count = RefBits((unsigned char) RCType::GC);
        frame->type = IncRef((TypeInfo *) type_frame_);

        Track(frame);

        new(&frame->lock)argon::vm::sync::Mutex();

        frame->flags = FrameFlags::CLEAR;
        frame->routine = nullptr;
        frame->back = nullptr;
        frame->globals = IncRef(globals);
        frame->proxy_globals = IncRef(proxy);
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

void argon::object::FrameFill(Frame *frame, Function *callable, ArObject **argv, ArSize argc) {
    ArSize local_idx = 0;

    // Push currying args
    if (callable->currying != nullptr) {
        for (ArSSize i = 0; i < callable->currying->len; i++)
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
