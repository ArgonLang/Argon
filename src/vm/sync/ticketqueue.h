// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_TICKETQUEUE_H_
#define ARGON_VM_SYNC_TICKETQUEUE_H_

#include <mutex>

#include <vm/arroutine.h>

namespace argon::vm::sync {

    class ArRoutineNotifyQueue {
        ArRoutine *head_ = nullptr;
        ArRoutine *tail_ = nullptr;
        std::mutex notify_lock_;

        std::atomic_ulong next_ = 0;
        std::atomic_ulong wait_ = 0;

    public:
        ArRoutineNotifyQueue() = default;

        ArRoutine *Notify();

        ArRoutine *NotifyAll();

        bool Wait(ArRoutine *routine, unsigned int ticket);

        inline bool IsTicketExpired(unsigned int ticket) {
            if (ticket == this->next_) {
                this->next_++;
                return true;
            } else if (ticket < this->next_)
                return true;

            return false;
        }

        unsigned int GetTicket();
    };

} // namespace argon::vm::sync

#endif // !ARGON_VM_SYNC_TICKETQUEUE_H_
