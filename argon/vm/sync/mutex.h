// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SYNC_MUTEX_H_
#define ARGON_VM_SYNC_MUTEX_H_

#include <atomic>

#include <argon/vm/sync/notifyqueue.h>

namespace argon::vm::sync {
    class Mutex {
        NotifyQueue queue_;

        std::atomic_uintptr_t lock_{};

        std::atomic_uintptr_t dirty_{};
    public:
        bool Lock();

        void Release();
    };
} // namespace argon::vm::sync

#endif // !ARGON_VM_SYNC_MUTEX_H_
