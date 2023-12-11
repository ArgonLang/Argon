// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RSM_H_
#define ARGON_VM_RSM_H_

#include <atomic>
#include <cassert>
#include <thread>

namespace argon::vm::sync {
    using MutexWord = unsigned int;

    class MutexBits {
        MutexWord bits{};

    public:
        [[nodiscard]] bool is_ulocked() const {
            return (this->bits & 0x01) == 1;
        }

        [[nodiscard]] MutexWord value() const {
            return this->bits;
        }

        void acquire_unique() {
            this->bits |= 1;
        }

        void dec_shared() {
            assert((this->bits >> 1) > 0);

            this->bits -= 0x01 << 1;
        }

        void inc_shared() {
            this->bits += 0x01 << 1;
        }

        void release_unique() {
            this->bits &= ~1;
        }
    };

    /**
     * Normally mutex do not allow unlock to be called by a different thread than the one that requested the lock.
     * This represents a problem for the Argon VM, let's imagine the following scenario:
     *
     * Thread A gets a buffer from an object (e.g. type Bytes) and makes a recv call on a socket.
     * Since there is no data available, the VM inserts the current fiber into the event loop.
     *
     * Thread B that manages the event loop will execute the recv on the waiting fiber, at the end of the call
     * the used buffer will have to be released through a call to mutex.unlock.
     * Since the locking thread is different from the unlocking thread, the use of standard mutex is not ALLOWED.
     *
     * This mutex implementation solves the problem and allows the following operations:
     *
     * - Lock from one thread and unlock from a different thread.
     * - Recursive lock by a thread (the call to unlock will have to match the number of calls to lock
     *   and can only be executed by the same thread).
     * - Shared lock (thread that acquired the unique lock can also request the shared lock).
     */
    class RecursiveSharedMutex {
        std::atomic<MutexBits> _lock{};
        std::atomic<MutexWord> _pending{};

        std::thread::id _id{};

        MutexWord _r_count{};

        void lock_shared_slow(std::thread::id id);

        void lock_slow();

    public:
        void lock();

        void lock_shared();

        void unlock();

        void unlock_shared();
    };
} // namespace argon::vm::sync

#endif // !ARGON_VM_RSM_H_
