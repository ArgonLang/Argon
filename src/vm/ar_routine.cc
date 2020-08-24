// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "ar_routine.h"

using namespace argon::vm;
using namespace argon::memory;

ArRoutine *argon::vm::RoutineNew(Frame *frame, ArRoutineStatus status) {
    auto routine = (ArRoutine *) Alloc(sizeof(ArRoutine));

    if (routine != nullptr) {
        routine->next = nullptr;
        routine->prev = nullptr;
        routine->frame = frame;
        routine->status = status;
    }

    return routine;
}

// *** ArRoutineQueue ***

ArRoutine *ArRoutineQueue::Dequeue() {
    ArRoutine *routine = this->front_;

    if (routine != nullptr) {
        this->front_ = routine->prev;
        if (routine->prev != nullptr)
            routine->prev->next = nullptr;
        else
            this->queue_ = nullptr;
        this->len_--;
    }

    return routine;
}

bool ArRoutineQueue::Enqueue(ArRoutine *routine) {
    if (this->len_ + 1 > ARGON_VM_QUEUE_MAX_ROUTINES)
        return false;

    routine->next = this->queue_;
    routine->prev = nullptr;

    if (this->queue_ != nullptr)
        this->queue_->prev = routine;
    else
        this->front_ = routine;

    this->queue_ = routine;

    this->len_++;
    return true;
}

ArRoutine *ArRoutineQueue::StealQueue(unsigned int min_len, ArRoutineQueue &queue) {
    if (this->GrabHalfQueue(min_len, queue) > 0)
        return this->Dequeue();
    return nullptr;
}

unsigned int ArRoutineQueue::GrabHalfQueue(unsigned int min_len, ArRoutineQueue &queue) {
    ArRoutine *mid = queue.queue_;  // Mid element pointer
    ArRoutine *last;                // Pointer to last element in queue
    unsigned int grab_len = 0;
    unsigned int counter = 0;

    // Check target queue minimum length
    if (queue.len_ < min_len) {
        return 0;
    }

    // StealQueue
    for (ArRoutine *cursor = queue.queue_; cursor != nullptr; cursor = cursor->next) {
        last = cursor;

        if (counter & (unsigned char) 1) {
            mid = mid->next;
            grab_len = counter;
        }
        counter++;
    }
    grab_len = queue.len_ - grab_len;

    queue.front_ = mid->prev;
    if (mid->prev != nullptr)
        mid->prev->next = nullptr;
    else
        queue.queue_ = nullptr;
    queue.len_ -= grab_len;

    last->next = this->queue_;
    if (this->queue_ != nullptr)
        this->queue_->prev = last;
    this->queue_ = mid;
    this->queue_->prev = nullptr;
    if (this->front_ == nullptr)
        this->front_ = last;
    this->len_ += grab_len;

    return grab_len;
}
