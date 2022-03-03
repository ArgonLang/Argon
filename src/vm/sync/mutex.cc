// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "mutex.h"

using namespace argon::vm;
using namespace argon::vm::sync;

bool Mutex::LockSlow() {
    auto current = false;

    if (this->flags_.compare_exchange_strong(current, true))
        return true;

    if (argon::vm::CanSpin()) {
        current = false;

        while (!this->flags_.compare_exchange_strong(current, true)) {
            current = false;
            if (this->queue_.Wait(GetRoutine(), this->queue_.GetTicket())) {
                UnschedRoutine(true, 0);
                return false;
            }
        }

        return true;
    }

    // Slow
    ReleaseQueue();

    current = false;
    if (this->flags_.compare_exchange_strong(current, true))
        return true;

    std::unique_lock lock(this->lock_);
    this->cond_.wait(lock, [this, &current] {
        current = false;
        return this->flags_.compare_exchange_strong(current, true);
    });

    return true;
}

bool Mutex::Lock() {
    auto current = false;

    if (!this->flags_.compare_exchange_strong(current, true))
        return this->LockSlow();

    return true;
}

bool Mutex::Unlock() {
    auto current = true;
    ArRoutine *routine;

    if (!this->flags_.compare_exchange_strong(current, false))
        return false;

    if ((routine = this->queue_.Notify()) != nullptr)
        Spawn(routine);
    else
        this->cond_.notify_one();

    return true;
}
