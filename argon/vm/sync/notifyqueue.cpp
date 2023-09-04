// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/sync/notifyqueue.h>

using namespace argon::vm;
using namespace argon::vm::sync;

bool NotifyQueue::Wait(FiberStatus status) {
    Fiber *fiber = GetFiber();

    std::unique_lock lock(this->lock_);

    if (!this->queue.Enqueue(fiber))
        return true; // No wait needed!

    lock.unlock();

    SetFiberStatus(status);

    return false;
}

void NotifyQueue::Notify() {
    std::unique_lock lock(this->lock_);

    auto *fiber = this->queue.Dequeue();

    lock.unlock();

    if (fiber != nullptr)
        vm::Spawn(fiber);
}

void NotifyQueue::NotifyAll() {
    std::unique_lock lock(this->lock_);

    Fiber *fiber = this->queue.Dequeue();

    while (fiber != nullptr) {
        vm::Spawn(fiber);

        fiber = this->queue.Dequeue();
    }
}