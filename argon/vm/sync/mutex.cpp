// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/sync/mutex.h>

using namespace argon::vm;
using namespace argon::vm::sync;

bool argon::vm::sync::Mutex::Lock() {
    const auto *fiber = vm::GetFiber();
    uintptr_t desired;

    do {
        desired = 0;

        if (this->lock_.compare_exchange_strong(desired, (uintptr_t) fiber))
            return true;
    } while (this->queue_.Wait());

    SetFiberStatus(FiberStatus::BLOCKED_SUSPENDED);

    this->dirty_++;

    return false;
}

void argon::vm::sync::Mutex::Release() {
    auto desired = (uintptr_t) vm::GetFiber();

    if (this->lock_.compare_exchange_strong(desired, 0)) {
        if (this->dirty_ != 0) {
            this->dirty_--;
            this->queue_.Notify();
        }

        return;
    }

    assert(false);
}
