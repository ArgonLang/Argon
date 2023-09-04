// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/fiber.h>
#include <argon/vm/datatype/nil.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

Frame *Fiber::FrameAlloc(unsigned int size, bool floating) {
    auto *ret = (Frame *) this->stack_cur;
    auto requested = sizeof(Frame) + (size * sizeof(void *));

    bool on_stack = false;

    if (!floating && ((((unsigned char *) this->stack_cur) + requested) < this->stack_end)) {
        this->stack_cur = (((unsigned char *) this->stack_cur) + requested);

        on_stack = true;
    } else
        ret = (Frame *) memory::Alloc(requested);

    if (ret != nullptr) {
        memory::MemoryZero(ret, sizeof(Frame));

        if (on_stack)
            ret->fiber_id = (ArSize) this;
    }

    return ret;
}

void Fiber::FrameDel(Frame *frame) {
    assert(((ArSize) this) == frame->fiber_id);

    auto subtract = (unsigned long) (((unsigned char *) this->stack_cur) - ((unsigned char *) frame));

    assert(subtract < ((uintptr_t) this));

    this->stack_cur = ((unsigned char *) this->stack_cur) - subtract;
}

Fiber *argon::vm::FiberNew(Context *context, unsigned int stack_space) {
    auto *fiber = (Fiber *) memory::Alloc(sizeof(Fiber) + stack_space);

    if (fiber != nullptr) {
        memory::MemoryZero(fiber, sizeof(Fiber));

        fiber->status = FiberStatus::RUNNABLE;

        fiber->context = context;

        fiber->stack_cur = fiber->stack_begin;

        fiber->stack_end = ((unsigned char *) fiber->stack_begin) + stack_space;
    }

    return fiber;
}

Frame *argon::vm::FrameNew(Fiber *fiber, Code *code, Namespace *globals, bool floating) {
    auto slots = code->stack_sz + code->sstack_sz;
    ArSize locals_sz = 0;

    if (code->locals != nullptr) {
        locals_sz = code->locals->length;
        slots += locals_sz;
    }

    auto *frame = fiber->FrameAlloc(slots, floating);
    if (frame == nullptr)
        return nullptr;

    frame->counter = 1;

    frame->globals = IncRef(globals);
    frame->code = IncRef(code);

    frame->return_value = nullptr;

    frame->instr_ptr = (unsigned char *) code->instr;

    frame->eval_stack = frame->extra;
    frame->locals = frame->extra + code->stack_sz;
    frame->sync_keys = frame->locals + locals_sz;

    // Set locals slots to nullptr
    slots = (slots - code->stack_sz) - code->sstack_sz;
    while (slots-- > 0)
        frame->locals[slots] = nullptr;

    return frame;
}

Frame *FrameWrapFnNew(Fiber *fiber, Function *func, ArObject **argv, ArSize argc, OpCodeCallMode mode) {
    auto *code = CodeWrapFnCall(argc, mode);
    if (code == nullptr)
        return nullptr;

    auto *frame = FrameNew(fiber, code, nullptr, false);
    if (frame == nullptr) {
        Release(code);
        return nullptr;
    }

    *(frame->eval_stack++) = (ArObject *) IncRef(func);

    for (int i = 0; i < argc; i++)
        *(frame->eval_stack++) = IncRef(argv[i]);

    return frame;
}

Frame *argon::vm::FrameNew(Fiber *fiber, Function *func, ArObject **argv, ArSize argc, OpCodeCallMode mode) {
    const Code *code = func->code;
    Dict *kwargs = nullptr;
    List *rest = nullptr;

    Frame *frame;

    unsigned short index_locals = 0;
    unsigned short index_argv = 0;
    unsigned short remains;

    if (func->IsNative())
        return FrameWrapFnNew(fiber, func, argv, argc, mode);

    if ((frame = FrameNew(fiber, func->code, func->gns, func->IsGenerator())) == nullptr)
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
        kwargs = (Dict *) argv[argc - 1];

        // Could be nil
        if ((ArObject *) kwargs == (ArObject *) Nil)
            kwargs = nullptr;

        argc--;
    }

    assert(argc >= remains); // argc cannot be less than remains

    while (index_argv < remains)
        frame->locals[index_locals++] = IncRef(argv[index_argv++]);

    // Push default arguments (If any...)
    if (func->default_args != nullptr) {
        for (auto i = 0; i < func->default_args->length; i++) {
            ArObject *value = nullptr;

            if (kwargs != nullptr)
                value = DictLookup(kwargs, code->locals->objects[index_locals]);

            if (value != nullptr)
                frame->locals[index_locals++] = value;
            else
                frame->locals[index_locals++] = IncRef(func->default_args->objects[i]);
        }
    }

    if (index_argv < argc) {
        if ((rest = ListNew(argc - index_argv)) == nullptr) {
            FrameDel(frame);
            return nullptr;
        }

        while (index_argv < argc)
            ListAppend(rest, argv[index_argv++]);
    }

    // Push enclosed (closure)
    frame->enclosed = IncRef(func->enclosed);

    if (func->IsMethod())
        frame->instance = *frame->locals;

    if (func->IsVariadic())
        frame->locals[index_locals++] = NilOrValue((ArObject *) rest);

    if (func->IsKWArgs())
        frame->locals[index_locals++] = NilOrValue(IncRef((ArObject *) kwargs));

    return frame;
}

void argon::vm::FiberDel(Fiber *fiber) {
    assert(fiber->frame == nullptr);

    Release(fiber->future);
    Release(fiber->references);

    memory::Free(fiber);
}

void argon::vm::FrameDel(Frame *frame) {
    const auto *code = frame->code;
    auto **locals_end = frame->locals;

    if (--frame->counter > 0)
        return;

    if (frame->back != nullptr)
        frame->back->counter--;

    if (code->locals != nullptr) {
        for (ArSize i = 0; i < code->locals->length; i++) {
            Release(locals_end);
            locals_end++;
        }
    }

    while (frame->sync_keys != locals_end) {
        MonitorRelease(*(frame->sync_keys - 1));

        Release(*(--frame->sync_keys));
    }

    Release(code);
    Release(frame->globals);
    Release(frame->enclosed);
    Release(frame->return_value);

    if (frame->gen_status != nullptr)
        *frame->gen_status = nullptr;

    if (frame->fiber_id == 0) {
        memory::Free(frame);
        return;
    }

    ((Fiber *) frame->fiber_id)->FrameDel(frame);
}

void argon::vm::FrameDelRec(Frame *frame) {
    Frame *back;

    if (frame == nullptr)
        return;

    do {
        back = frame->back;

        FrameDel(frame);

        frame = back;
    } while (frame != nullptr && frame->counter == 0);
}