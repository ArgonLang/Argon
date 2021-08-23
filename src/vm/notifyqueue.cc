// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "notifyqueue.h"

using namespace argon::vm;

ArRoutine *ArRoutineNotifyQueue::Notify() {
    ArRoutine *back = nullptr;
    unsigned int ticket;

    if (this->wait_ == this->next_)
        return nullptr;

    std::unique_lock lock(this->notify_lock_);

    if (this->wait_ == this->next_)
        return nullptr;

    ticket = this->next_.fetch_add(1);

    for (ArRoutine *cursor = this->head_; cursor != nullptr; back = cursor, cursor = cursor->next) {
        if (cursor->ticket == ticket) {
            if (back != nullptr)
                back->next = cursor->next;
            else
                this->head_ = cursor->next;

            if (cursor->next == nullptr)
                this->tail_ = back;

            cursor->next = nullptr;
            return cursor;
        }
    }

    return nullptr;
}

ArRoutine *ArRoutineNotifyQueue::NotifyAll() {
    ArRoutine *routines;

    if (this->wait_ == this->next_)
        return nullptr;

    std::unique_lock lock(this->notify_lock_);

    routines = this->head_;
    this->head_ = nullptr;
    this->tail_ = nullptr;

    this->next_.store(this->wait_);

    return routines;
}

bool ArRoutineNotifyQueue::Wait(ArRoutine *routine, unsigned int ticket) {
    std::unique_lock lock(this->notify_lock_);

    if (ticket < this->next_)
        return false;

    // Enqueue
    routine->ticket = ticket;

    if (this->tail_ == nullptr)
        this->head_ = routine;
    else
        this->tail_->next = routine;

    this->tail_ = routine;

    return true;
}

unsigned int ArRoutineNotifyQueue::GetTicket() {
    return this->wait_.fetch_add(1);
}