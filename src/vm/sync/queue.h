// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_QUEUE_H_
#define ARGON_VM_SYNC_QUEUE_H_

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <vm/sync/ticketqueue.h>
#include <vm/runtime.h>

namespace argon::vm::sync {
    class Queue {
        ArRoutineNotifyQueue queue_;

        std::condition_variable cond_;
        std::mutex lock_;

    public:
        bool Enqueue(bool resume, unsigned long reason, unsigned int ticket);

        inline bool Enqueue(bool resume, unsigned long reason) {
            return this->Enqueue(resume, reason, this->queue_.GetTicket());
        }

        inline unsigned int GetTicket() { return this->queue_.GetTicket(); }

        void Notify();

        void Broadcast();
    };
}

#endif // !ARGON_VM_SYNC_QUEUE_H_
