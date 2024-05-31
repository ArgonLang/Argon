// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FIBER_H_
#define ARGON_VM_FIBER_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/namespace.h>

#include <argon/vm/sync/mcond.h>
#include <argon/vm/sync/sync.h>

#include <argon/vm/context.h>
#include <argon/vm/fstatus.h>
#include <argon/vm/frame.h>
#include <argon/vm/opcode.h>
#include <argon/vm/panic.h>

namespace argon::vm::datatype {
    struct Future;
}

namespace argon::vm {
    constexpr const unsigned short kFiberStackSize = 1024; // 1KB
    constexpr const unsigned short kFiberPoolSize = 254; // Items

    struct Fiber {
        /// Routine status.
        FiberStatus status;

        /// Pointer to a stack allocated MCond object (see EvalSync).
        sync::MCond *sync_cv;

        sync::NotifyQueueTicket ticket;

        struct {
            Fiber *next;
            Fiber *prev;
        } rq;

        Context *context;

        datatype::ArObject *async_result;

        /// Current execution frame.
        Frame *frame;

        datatype::Future *future;

        /// Stores object references of a function that may become recursive (e.g. list_repr, dict_repr...)
        datatype::List *references;

        /// Pointer to object that describe actual routine panic (if any...).
        struct Panic *panic;

        /// Raw pointer to the OSThread running this fiber (only used to check if the fiber is already running on an OSThread).
        void *active_ost;

        /// Pointer to the frame allocated by the last EvalSync call.
        void *unwind_limit;

        void *stack_cur;

        void *stack_end;

        void *stack_begin[];

        Frame *FrameAlloc(unsigned int size, bool floating);

        void FrameDel(Frame *frame);
    };

    Fiber *FiberNew(Context *context, unsigned int stack_space);

    Frame *FrameNew(Fiber *fiber, datatype::Code *code, datatype::Namespace *globals, bool floating);

    Frame *FrameNew(Fiber *fiber, datatype::Function *func, datatype::ArObject **argv, datatype::ArSize argc,
                    OpCodeCallMode mode);

    inline Frame *FiberPopFrame(Fiber *fiber) {
        auto *popped = fiber->frame;
        fiber->frame = popped->back;

        return popped;
    }

    inline void FiberSetAsyncResult(Fiber *fiber, datatype::ArObject *result) {
        datatype::Release(fiber->async_result);
        fiber->async_result = datatype::IncRef(result);
    }

    void FiberDel(Fiber *fiber);

    void FrameDel(Frame *frame);

    void FrameDelRec(Frame *frame);

    inline void FiberPushFrame(Fiber *fiber, Frame *_new) {
        _new->back = fiber->frame;

        if (fiber->frame != nullptr)
            fiber->frame->counter++;

        fiber->frame = _new;
    }

} // namespace argon::vm

#endif // !ARGON_VM_FIBER_H_
