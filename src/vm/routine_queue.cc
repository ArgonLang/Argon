// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "routine_queue.h"

using namespace argon::vm;

bool ArRoutineQueue::Enqueue(ArRoutine *routine) {
    if (this->limit_ > 0 && this->len_ + 1 >= this->limit_)
        return false;

    if (this->tail_ == nullptr) {
        this->head_ = routine;
        this->tail_ = routine;
    } else {
        this->tail_->next = routine;
        this->tail_ = routine;
    }

    this->len_++;

    return true;
}

ArRoutine *ArRoutineQueue::Dequeue() {
    ArRoutine *routine = this->head_;

    if (routine != nullptr) {
        this->head_ = routine->next;
        this->len_--;
    }

    return routine;
}

ArRoutine *ArRoutineQueue::StealQueue(unsigned int min_len, ArRoutineQueue &queue) {
    if (this->GrabHalfQueue(min_len, queue) > 0)
        return this->Dequeue();
    return nullptr;
}

unsigned int ArRoutineQueue::GrabHalfQueue(unsigned int min_len, ArRoutineQueue &queue) {
    ArRoutine *mid = queue.head_;   // Mid element pointer
    ArRoutine *mid_prev = nullptr;
    ArRoutine *last;                // Pointer to last element in queue
    unsigned int grab_len = 0;
    unsigned int counter = 0;

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