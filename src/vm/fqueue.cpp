// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "fqueue.h"

using namespace argon::vm;

Fiber *FiberQueue::Dequeue() {
    std::unique_lock lock(this->lock_);

    Fiber *ret = this->head_;

    if (ret != nullptr) {
        this->head_ = ret->rq.prev;

        if (this->head_ == nullptr)
            this->tail_ = nullptr;

        this->items_--;
    }

    return ret;
}

bool FiberQueue::Enqueue(argon::vm::Fiber *fiber) {
    std::unique_lock lock(this->lock_);

    if (this->max_ > 0 && (this->items_ + 1 >= this->max_))
        return false;

    fiber->rq.next = this->tail_;
    fiber->rq.prev = nullptr;

    if (this->tail_ == nullptr)
        this->head_ = fiber;
    else
        this->tail_->rq.prev = fiber;

    this->tail_ = fiber;

    this->items_++;

    return true;
}

bool FiberQueue::IsEmpty() {
    std::unique_lock lock(this->lock_);
    return this->items_ == 0;
}

bool FiberQueue::InsertHead(argon::vm::Fiber *fiber) {
    std::unique_lock lock(this->lock_);

    if (this->max_ > 0 && (this->items_ + 1 >= this->max_))
        return false;

    if (this->head_ == nullptr) {
        this->head_ = fiber;
        this->tail_ = fiber;
        this->items_++;

        return true;
    }

    this->head_->rq.next = fiber;
    fiber->rq.prev = this->head_;

    this->head_ = fiber;

    this->items_++;

    return true;
}

void FiberQueue::Relinquish(argon::vm::Fiber *fiber) {
    std::unique_lock lock(this->lock_);

    if (fiber->rq.prev != nullptr)
        fiber->rq.prev->rq.next = fiber->rq.next;

    if (fiber->rq.next != nullptr)
        fiber->rq.next->rq.prev = fiber->rq.prev;

    if (this->tail_ == fiber)
        this->tail_ = fiber->rq.next;

    if (this->head_ == fiber)
        this->head_ = fiber->rq.prev;

    this->items_--;
}
