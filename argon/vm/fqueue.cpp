// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/fqueue.h>

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

Fiber *FiberQueue::StealDequeue(unsigned short min_len, argon::vm::FiberQueue &queue) {
    if (this->StealHalf(min_len, queue) > 0)
        return this->Dequeue();

    return nullptr;
}

bool FiberQueue::Enqueue(argon::vm::Fiber *fiber) {
    if (fiber == nullptr)
        return true;

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
    if (fiber == nullptr)
        return true;

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

unsigned int FiberQueue::StealHalf(unsigned short min_len, argon::vm::FiberQueue &queue) {
    std::unique_lock lock(this->lock_);
    std::unique_lock lock_other(queue.lock_);

    Fiber *mid = queue.tail_;   // Mid element pointer
    Fiber *last = queue.head_;  // Pointer to last element in queue
    Fiber *mid_prev = nullptr;
    unsigned int counter = 0;
    unsigned int grab_len;

    // Check target queue minimum length
    if (queue.items_ == 0 || queue.items_ < min_len)
        return 0;

    // Steal half queue
    for (Fiber *cursor = queue.tail_; cursor != nullptr; cursor = cursor->rq.next) {
        last = cursor;

        if (counter & 1u) {
            mid_prev = mid;
            mid = mid->rq.next;
        }

        counter++;
    }

    grab_len = (queue.items_ / 2) + (queue.items_ & 1u);

    if (queue.tail_ == queue.head_)
        queue.tail_ = nullptr;

    queue.head_ = mid_prev;
    if (mid_prev != nullptr)
        mid_prev->rq.next = nullptr;
    queue.items_ -= grab_len;

    mid->rq.prev = nullptr;

    if (this->tail_ == nullptr) {
        this->tail_ = mid;
        this->head_ = last;
    } else {
        this->tail_->rq.prev = last;
        last->rq.next = this->tail_;
        this->tail_ = mid;
    }

    this->items_ += grab_len;

    return grab_len;
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
