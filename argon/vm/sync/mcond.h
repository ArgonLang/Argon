// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_MCOND_H_
#define ARGON_VM_SYNC_MCOND_H_

#include <condition_variable>
#include <mutex>

namespace argon::vm::sync {
    class MCond {
        std::mutex lock_;
        std::condition_variable cond_;

    public:
        void Notify() {
            this->cond_.notify_one();
        }

        template<typename Predicate>
        void wait(Predicate pred) {
            std::unique_lock lck(this->lock_);
            this->cond_.wait(lck, pred);
        }
    };

} // namespace argon::vm::sync

#endif // !ARGON_VM_SYNC_MCOND_H_
