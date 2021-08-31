// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "mutex.h"

using namespace argon::vm;
using namespace argon::vm::sync;

bool Mutex::LockSlow(unsigned long expected, unsigned long with_value) {
    auto current = expected;
    auto spin = CanSpin();

    std::unique_lock lock(this->lock_);

    if (this->flags_.compare_exchange_strong(current, with_value))
        return true;

    if (spin) {
        this->queue_.Enqueue(UnschedRoutine(true));
        return false;
    }

    ReleaseQueue();

    this->blocked_++;
    this->cond_.wait(lock, [this, &current, expected, with_value] {
        current = expected;
        return this->flags_.compare_exchange_strong(current, with_value);
    });
    this->blocked_--;

    return true;
}

bool Mutex::Lock(unsigned long expected, unsigned long with_value) {
    auto current = expected;

    if (!this->flags_.compare_exchange_strong(current, with_value))
        return this->LockSlow(expected, with_value);

    return true;
}

bool Mutex::Unlock(unsigned long expected, unsigned long with_value) {
    auto current = expected;
    ArRoutine *routine;

    if (!this->flags_.compare_exchange_strong(current, with_value))
        return false;

    std::unique_lock lock(this->lock_);

    if (this->blocked_ == 0) {
        routine = this->queue_.Dequeue();

        if (routine != nullptr)
            Spawn(routine);
    } else
        this->cond_.notify_one();

    return true;
}
