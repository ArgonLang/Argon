// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "queue.h"

using namespace argon::vm::sync;

bool Queue::Enqueue(bool resume, unsigned long reason, unsigned int ticket) {
    ArRoutine *routine;

    if (CanSpin()) {
        routine = GetRoutine();

        if (this->queue_.Wait(routine, ticket)) {
            argon::vm::UnschedRoutine(resume, reason);
            return true;
        }
    } else {
        ReleaseQueue();

        std::unique_lock lock(this->lock_);
        this->cond_.wait(lock, [this, ticket] {
            return this->queue_.IsTicketExpired(ticket);
        });
    }

    return false;
}

void Queue::Notify() {
    argon::vm::ArRoutine *routine;

    if ((routine = this->queue_.Notify()) != nullptr) {
        Spawn(routine);
        return;
    }

    this->cond_.notify_all();
}

void Queue::Broadcast() {
    ArRoutine *routines = this->queue_.NotifyAll();

    this->cond_.notify_all();
    Spawns(routines);
}
