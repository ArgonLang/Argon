// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_MUTEX_H_
#define ARGON_VM_SYNC_MUTEX_H_

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "ticketqueue.h"

namespace argon::vm::sync {
    class Mutex {
        std::mutex lock_;
        std::condition_variable cond_;
        ArRoutineNotifyQueue queue_;
        std::atomic_bool flags_;

        bool LockSlow();

    public:
        bool Lock();

        bool Unlock();
    };
} // namespace argon::vm::sync

#endif // !ARGON_VM_SYNC_MUTEX_H_
