// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "fiber.h"
#include "vm/datatype/nil.h"

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

Frame *argon::vm::FrameNew(Fiber *fiber, Function *func, ArObject **argv, ArSize argc, OpCodeCallMode mode) {
    ArObject *kwargs = nullptr;
    List *rest = nullptr;
    Frame *frame;

    unsigned short index_locals = 0;
    unsigned short index_argv = 0;
    unsigned short remains;

    if ((frame = FrameNew(fiber, func->code, func->gns, !func->IsGenerator())) == nullptr)
        return nullptr;

    // Push currying args
    if (func->currying != nullptr) {
        for (ArSize i = 0; i < func->currying->length; i++)
            frame->locals[index_locals++] = IncRef(func->currying->objects[i]);
    }

    // Fill with stack args
    remains = func->arity - index_locals;

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS)) {
        // If mode == KW_PARAMS, the last element is the arguments dict
        kwargs = IncRef(argv[argc - 1]);
        argc--;
    }

    while (index_argv < remains && index_argv < argc)
        frame->locals[index_locals++] = IncRef(argv[index_argv++]);

    if (index_argv < argc) {
        if ((rest = ListNew(argc - index_argv)) == nullptr) {
            FrameDel(frame);
            return nullptr;
        }

        while (index_argv < argc)
            ListAppend(rest, argv[index_argv++]);
    }

    if (func->IsVariadic())
        frame->locals[index_locals++] = NilOrValue((ArObject *) rest);

    if (func->IsKWArgs())
        frame->locals[index_locals++] = NilOrValue(kwargs);

    return frame;
}

void argon::vm::FiberDel(Fiber *fiber) {
    assert(fiber->frame == nullptr);

    Release(fiber->future);

    memory::Free(fiber);
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