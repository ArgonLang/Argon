// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FSTATUS_H_
#define ARGON_VM_FSTATUS_H_

namespace argon::vm {
    enum class FiberStatus : unsigned long {
        /// Fiber is blocked and will be removed by the scheduler.
        /// The operation that changed the state to blocked must have saved the current fiber to its own queue.
        BLOCKED,
        /// Exactly like BLOCKED but in this case when spawn is called to start the fiber, the last opcode will be re-executed.
        BLOCKED_SUSPENDED,
        /// Fiber is ready to run.
        RUNNABLE,
        /// Fiber is running.
        RUNNING,
        /// Fiber has been suspended, the last opcode will be re-executed.
        /// This fiber will NOT be dequeued but will be moved to the last position.
        SUSPENDED
    };
} // namespace argon::vm

#endif // !ARGON_VM_FSTATUS_H_
