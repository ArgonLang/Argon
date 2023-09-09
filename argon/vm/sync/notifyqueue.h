// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_NOTIFYQUEUE_H_
#define ARGON_VM_SYNC_NOTIFYQUEUE_H_

#include <atomic>
#include <mutex>

#include <argon/vm/fstatus.h>

#include <argon/vm/sync/sync.h>
#include <argon/vm/sync/ticket.h>

namespace argon::vm::sync {
    class NotifyQueue {
        std::mutex lock_;

        Ticket queue;
    public:
        bool Wait(FiberStatus status);

        bool Wait() {
            return Wait(FiberStatus::BLOCKED);
        }

        void Notify();

        void NotifyAll();
    };
}

#endif // !ARGON_VM_SYNC_NOTIFYQUEUE_H_
