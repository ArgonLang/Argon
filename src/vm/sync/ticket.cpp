// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "ticket.h"

using namespace argon::vm;
using namespace argon::vm::sync;

bool NotifyQueue::Wait() {
    NotifyQueueTicket ticket = this->wait_.fetch_add(1);
    Fiber *fiber = GetFiber();

    std::unique_lock lock(this->lock_);

    if (ticket < this->next_)
        return true; // No wait needed!

    // Enqueue
    fiber->status = FiberStatus::BLOCKED;

    fiber->ticket = ticket;

    this->Enqueue(fiber);

    return false;
}

void NotifyQueue::Enqueue(argon::vm::Fiber *fiber) {
    fiber->rq.next = this->tail_;
    fiber->rq.prev = nullptr;

    if (this->tail_ == nullptr)
        this->head_ = fiber;
    else
        this->tail_->rq.prev = fiber;

    this->tail_ = fiber;
}

void NotifyQueue::Notify() {
    if (this->wait_ == this->next_)
        return;

    std::unique_lock lock(this->lock_);

    auto ticket = this->next_.fetch_add(1);

    for (Fiber *cursor = this->tail_; cursor != nullptr; cursor = cursor->rq.next) {
        if (cursor->ticket == ticket) {
            if (cursor->rq.next != nullptr)
                cursor->rq.next->rq.prev = cursor->rq.prev;
            else
                this->head_ = cursor->rq.prev;

            if (cursor->rq.prev != nullptr)
                cursor->rq.prev->rq.next = cursor->rq.next;
            else
                this->tail_ = cursor->rq.next;

            vm::Spawn(cursor);
            break;
        }
    }
}

void NotifyQueue::NotifyAll() {
    if (this->wait_ == this->next_)
        return;

    std::unique_lock lock(this->lock_);

    Fiber *next = nullptr;
    for (Fiber *cursor = this->tail_; cursor != nullptr; cursor = next) {
        next = cursor->rq.next;

        cursor->rq.next = nullptr;
        cursor->rq.prev = nullptr;

        vm::Spawn(cursor);
    }

    this->tail_ = nullptr;
    this->head_ = nullptr;

    this->next_.store(this->wait_);
}