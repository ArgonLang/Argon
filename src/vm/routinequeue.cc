// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "routinequeue.h"

using namespace argon::vm;

ArRoutine *ArRoutineQueue::Dequeue() {
    std::unique_lock<std::mutex> uniqueLock(this->queue_lock);

    ArRoutine *routine = this->head_;

    if (routine != nullptr) {
        this->head_ = routine->next;

        if (this->head_ == nullptr)
            this->tail_ = nullptr;

        this->len_--;

        routine->next = nullptr;
    }

    return routine;
}

ArRoutine *ArRoutineQueue::StealQueue(unsigned int min_len, ArRoutineQueue &queue) {
    if (this->GrabHalfQueue(min_len, queue) > 0)
        return this->Dequeue();
    return nullptr;
}

bool ArRoutineQueue::Enqueue(ArRoutine *routine) {
    std::unique_lock<std::mutex> uniqueLock(this->queue_lock);

    if (this->limit_ > 0 && this->len_ + 1 >= this->limit_)
        return false;

    if (this->tail_ == nullptr)
        this->head_ = routine;
    else
        this->tail_->next = routine;

    this->tail_ = routine;

    this->len_++;
    return true;
}

bool ArRoutineQueue::EnqueueHead(ArRoutine *routine) {
    std::unique_lock<std::mutex> uniqueLock(this->queue_lock);

    if (this->limit_ > 0 && this->len_ + 1 >= this->limit_)
        return false;

    if (this->tail_ == nullptr) {
        this->head_ = routine;
        this->tail_ = routine;
    } else {
        routine->next = this->head_;
        this->head_ = routine;
    }

    this->len_++;
    return true;
}

unsigned int ArRoutineQueue::GrabHalfQueue(unsigned int min_len, ArRoutineQueue &queue) {
    std::unique_lock<std::mutex> uniqueLock(this->queue_lock);

    ArRoutine *mid = queue.head_;   // Mid element pointer
    ArRoutine *last = queue.tail_;  // Pointer to last element in queue
    ArRoutine *mid_prev = nullptr;
    unsigned int counter = 0;
    unsigned int grab_len;

    // Check target queue minimum length
    if (queue.len_ < min_len) {
        return 0;
    }

    // StealQueue
    for (ArRoutine *cursor = queue.head_; cursor != nullptr; cursor = cursor->next) {
        last = cursor;

        if (counter & (unsigned char) 1) {
            mid_prev = mid;
            mid = mid->next;
        }
        counter++;
    }

    grab_len = (queue.len_ / 2) + (queue.len_ & (unsigned char) 1);

    if (queue.head_ == queue.tail_)
        queue.head_ = mid_prev;

    queue.tail_ = mid_prev;
    if (mid_prev != nullptr)
        mid_prev->next = nullptr;
    queue.len_ -= grab_len;

    if (this->tail_ != nullptr)
        this->tail_->next = mid;
    else
        this->head_ = mid;

    this->tail_ = last;
    this->len_ += grab_len;

    return grab_len;
}
