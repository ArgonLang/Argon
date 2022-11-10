// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FIBER_H_
#define ARGON_VM_FIBER_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/code.h>
#include <vm/datatype/future.h>
#include <vm/datatype/namespace.h>

#include "panic.h"
#include "frame.h"

namespace argon::vm {
    constexpr const unsigned short kFiberStackSize = 1024; // 1KB
    constexpr const unsigned short kFiberPoolSize = 254; // Items

    enum class FiberStatus : unsigned char {
        BLOCKED,
        RUNNABLE,
        RUNNING,
        SUSPENDED
    };

    struct Fiber {
        struct {
            Fiber *next;
            Fiber *prev;
        } rq;

        datatype::Future *future;

        /// Current execution frame.
        Frame *frame;

        /// Pointer to object that describe actual routine panic (if any...).
        struct Panic *panic;

        /// Routine status.
        FiberStatus status;

        void *stack_cur;

        void *stack_end;

        void *stack_begin[];

        Frame *FrameAlloc(bool *out_onstack, unsigned int size, bool floating);

        void FrameDel(Frame *frame);
    };

    Fiber *FiberNew(unsigned int stack_space);

    Frame *FrameNew(Fiber *fiber, datatype::Code *code, datatype::Namespace *globals, bool floating);

    void FiberDel(Fiber *fiber);

    void FrameDel(Frame *frame);

} // namespace argon::vm

#endif // !ARGON_VM_FIBER_H_
