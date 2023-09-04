// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/sync/ticket.h>

using namespace argon::vm;
using namespace argon::vm::sync;

bool Ticket::Enqueue(Fiber *fiber) {
    NotifyQueueTicket ticket = this->wait_.fetch_add(1);

    if (ticket < this->next_)
        return false;

    fiber->ticket = ticket;

    //                                            +----head
    //                                            v
    //           +--------+    +--------+    +--------+
    //           |        |    |        |    |        |
    // tail ---> |  obj3  +--->|  obj2  +--->|  obj1  |
    //           |        |    |        |    |        |
    //           +--------+    +--------+    +--------+

    fiber->rq.next = this->tail_;
    fiber->rq.prev = nullptr;

    if (this->tail_ == nullptr)
        this->head_ = fiber;
    else
        this->tail_->rq.prev = fiber;

    this->tail_ = fiber;

    return true;
}

Fiber *Ticket::Dequeue() {
    if (this->wait_ != this->next_) {
        auto ticket = this->next_.fetch_add(1);

        for (auto *cursor = this->tail_; cursor != nullptr; cursor = cursor->rq.next) {
            if (cursor->ticket != ticket)
                continue;

            if (cursor->rq.next != nullptr)
                cursor->rq.next->rq.prev = cursor->rq.prev;
            else
                this->head_ = cursor->rq.prev;

            if (cursor->rq.prev != nullptr)
                cursor->rq.prev->rq.next = cursor->rq.next;
            else
                this->tail_ = cursor->rq.next;

            return cursor;
        }
    }

    return nullptr;
}