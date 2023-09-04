// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_TICKET_H_
#define ARGON_VM_SYNC_TICKET_H_

#include <atomic>

namespace argon::vm {
    struct Fiber;
}

namespace argon::vm::sync {
    class Ticket {
        Fiber *head_ = nullptr;
        Fiber *tail_ = nullptr;

        std::atomic_ulong next_ = 0;
        std::atomic_ulong wait_ = 0;

    public:
        bool Enqueue(Fiber *fiber);

        Fiber *Dequeue();
    };
}

#endif // !ARGON_VM_SYNC_TICKET_H_
