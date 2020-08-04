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
