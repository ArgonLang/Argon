// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_MUTEX_H_
#define ARGON_VM_SYNC_MUTEX_H_

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <vm/routinequeue.h>

namespace argon::vm::sync {

    enum class MutexStatus {
        UNLOCK,
        LOCK
    };

    class Mutex {
        std::mutex lock_;
        std::condition_variable cond_;
        argon::vm::ArRoutineQueue queue_;

        std::atomic<unsigned long> flags_;
        unsigned long blocked_;

        bool LockSlow(unsigned long expected, unsigned long with_value);

    public:
        bool Lock(unsigned long expected, unsigned long with_value);

        inline bool Lock() {
            return this->Lock((unsigned long) MutexStatus::UNLOCK, (unsigned long) MutexStatus::LOCK);
        }

        bool Unlock(unsigned long expected, unsigned long with_value);

        inline bool Unlock() {
            return this->Lock((unsigned long) MutexStatus::LOCK, (unsigned long) MutexStatus::UNLOCK);
        }
    };
} // namespace argon::vm::sync

#endif // !ARGON_VM_SYNC_MUTEX_H_
