// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_TICKET_H_
#define ARGON_VM_SYNC_TICKET_H_

#include <atomic>
#include <mutex>

#include <vm/fiber.h>

#include "sync.h"

namespace argon::vm::sync {
    class NotifyQueue {
        std::mutex lock_;

        Fiber *head_ = nullptr;
        Fiber *tail_ = nullptr;

        std::atomic_ulong next_ = 0;
        std::atomic_ulong wait_ = 0;

        void Enqueue(Fiber *fiber);

    public:
        bool Wait();

        void Notify();

        void NotifyAll();
    };
}

#endif // !ARGON_VM_SYNC_TICKET_H_
