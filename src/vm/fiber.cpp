// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "fiber.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

Frame *Fiber::FrameAlloc(bool *out_onstack, unsigned int size, bool floating) {
    auto *ret = (Frame *) this->stack_cur;
    auto requested = sizeof(Frame) + (size * sizeof(void *));

    *out_onstack = false;

    if (!floating && ((((unsigned char *) this->stack_cur) + requested) < this->stack_end)) {
        this->stack_cur = (((unsigned char *) this->stack_cur) + requested);

        *out_onstack = true;
    } else
        ret = (Frame *) memory::Alloc(requested);

    if (ret != nullptr)
        memory::MemoryZero(ret, sizeof(Frame));

    return ret;
}

void Fiber::FrameDel(Frame *frame) {
    assert(((ArSize) this) == frame->fiber_id);

    auto subtract = (unsigned long) (((unsigned char *) this->stack_cur) - ((unsigned char *) frame));

    assert(subtract < ((uintptr_t) this));

    this->stack_cur = ((unsigned char *) this->stack_cur) - subtract;
}

Fiber *argon::vm::FiberNew(unsigned int stack_space) {
    auto *fiber = (Fiber *) memory::Alloc(sizeof(Fiber) + (sizeof(void *) + stack_space));

    if (fiber != nullptr) {
        memory::MemoryZero(fiber, sizeof(Fiber));

        fiber->status = FiberStatus::RUNNABLE;

        fiber->stack_cur = fiber->stack_begin;

        fiber->stack_end = fiber->stack_begin + (sizeof(void *) + stack_space);
    }

    return fiber;
}

Frame *argon::vm::FrameNew(Fiber *fiber, Code *code, Namespace *globals, bool floating) {
    auto slots = code->stack_sz;
    bool on_stack;

    if (code->locals != nullptr)
        slots += code->locals->length;

    auto *frame = fiber->FrameAlloc(&on_stack, slots, floating);
    if (frame == nullptr)
        return nullptr;

    if (on_stack)
        frame->fiber_id = (ArSize) fiber;

    frame->globals = IncRef(globals);
    frame->code = IncRef(code);

    frame->instr_ptr = (unsigned char *) code->instr;

    frame->eval_stack = frame->extra;
    frame->locals = frame->extra + code->stack_sz;

    // Set locals slots to nullptr
    slots -= code->stack_sz;
    while (slots-- > 0)
        frame->locals[slots] = nullptr;

    return frame;
}

void argon::vm::FrameDel(Frame *frame) {
    const auto *code = frame->code;

    if (code->locals != nullptr) {
        for (ArSize i = 0; i < code->locals->length; i++)
            Release(frame->locals[i]);
    }

    Release(code);
    Release(frame->globals);
    Release(frame->enclosed);
    Release(frame->return_value);

    if (frame->fiber_id == 0) {
        memory::Free(frame);
        return;
    }

    ((Fiber *) frame->fiber_id)->FrameDel(frame);
}