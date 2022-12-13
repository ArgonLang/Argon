// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FIBER_H_
#define ARGON_VM_FIBER_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/code.h>
#include <vm/datatype/function.h>
#include <vm/datatype/namespace.h>

#include <vm/sync/sync.h>

#include "frame.h"
#include "opcode.h"
#include "panic.h"

namespace argon::vm::datatype {
    struct Future;
}

namespace argon::vm {
    constexpr const unsigned short kFiberStackSize = 1024; // 1KB
    constexpr const unsigned short kFiberPoolSize = 254; // Items

    enum class FiberStatus : unsigned long {
        BLOCKED,
        RUNNABLE,
        RUNNING,
        SUSPENDED
    };

    struct Fiber {
        /// Routine status.
        FiberStatus status;

        sync::NotifyQueueTicket ticket;

        struct {
            Fiber *next;
            Fiber *prev;
        } rq;

        /// Current execution frame.
        Frame *frame;

        datatype::Future *future;

        /// Stores object references of a function that may become recursive (e.g. list_repr, dict_repr...)
        datatype::List *references;

        /// Pointer to object that describe actual routine panic (if any...).
        struct Panic *panic;

        void *stack_cur;

        void *stack_end;

        void *stack_begin[];

        Frame *FrameAlloc(unsigned int size, bool floating);

        void FrameDel(Frame *frame);
    };

    Fiber *FiberNew(unsigned int stack_space);

    Frame *FrameNew(Fiber *fiber, datatype::Code *code, datatype::Namespace *globals, bool floating);

    Frame *FrameNew(Fiber *fiber, datatype::Function *func, datatype::ArObject **argv, datatype::ArSize argc,
                    OpCodeCallMode mode);

    inline Frame *FiberPopFrame(Fiber *fiber) {
        auto *popped = fiber->frame;
        fiber->frame = popped->back;

        return popped;
    }

    void FiberDel(Fiber *fiber);

    void FrameDel(Frame *frame);

    inline void FiberPushFrame(Fiber *fiber, Frame *_new) {
        _new->back = fiber->frame;
        fiber->frame = _new;
    }

} // namespace argon::vm

#endif // !ARGON_VM_FIBER_H_
