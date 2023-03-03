// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "util/macros.h"

#ifdef _ARGON_PLATFORM_DARWIN

#include <sys/event.h>

#include <vm/runtime.h>

#include <vm/memory/memory.h>

#include <vm/datatype/error.h>

#include "event.h"
#include "evloop.h"

using namespace argon::vm::loop;
using namespace argon::vm::datatype;

// Prototypes

void ProcessOutTrigger(EvLoop *loop);

void ProcessQueue(EventQueue *queue, EventDirection direction);

// EOL

EvLoop *argon::vm::loop::EventLoopNew() {
    EvLoop *evl;

    if ((evl = (EvLoop *) argon::vm::memory::Alloc(sizeof(EvLoop))) != nullptr) {
        if ((evl->handle = kqueue()) < 0) {
            ErrorFromErrno(errno);

            argon::vm::memory::Free(evl);

            return nullptr;
        }

        new(&evl->queue_lock)std::mutex();

        evl->out_queues = nullptr;
        evl->allocable_events = nullptr;
        evl->free_count = 0;
    }

    return evl;
}

bool argon::vm::loop::EventLoopIOPoll(EvLoop *loop, unsigned long timeout) {
    struct kevent events[kMaxEvents];
    struct kevent kev[2];
    timespec ts{};

    ProcessOutTrigger(loop);

    ts.tv_sec = (long) timeout / 1000;
    ts.tv_nsec = (long) ((timeout % 1000) * 1000000);

    auto ret = kevent(loop->handle, nullptr, 0, events, kMaxEvents, &ts);
    if (ret < 0) {
        if (errno == EINTR)
            return false; // Try again

        // Never get here!
        assert(false);
    }

    for (int i = 0; i < ret; i++) {
        auto *queue = (EventQueue *) events[i].udata;

        std::unique_lock _(queue->lock);

        if (events[i].filter == EVFILT_READ)
            ProcessQueue(queue, EventDirection::IN);
        else if (events[i].filter == EVFILT_WRITE)
            ProcessQueue(queue, EventDirection::OUT);

        if (queue->items == 0) {
            EV_SET(kev, queue->handle, events[i].filter, EV_DELETE, 0, 0, nullptr);

            if (kevent(loop->handle, kev, 1, nullptr, 0, nullptr) < 0)
                assert(false); // Never get here!
        }
    }

    return true;
}

bool argon::vm::loop::EventLoopAddEvent(EvLoop *loop, EventQueue *queue, Event *event, EventDirection direction) {
    struct kevent kev[2];

    std::unique_lock _(queue->lock);

    if (queue->items == 0) {
        EV_SET(kev, queue->handle, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, queue);
        EV_SET(kev + 1, queue->handle, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, queue);

        if (kevent(loop->handle, kev, 2, nullptr, 0, nullptr) < 0) {
            _.unlock();

            vm::SetFiberStatus(FiberStatus::RUNNING);

            ErrorFromErrno(errno);

            return false;
        }
    }

    if (direction == EventDirection::OUT && queue->out_event.head == nullptr) {
        queue->next = loop->out_queues;
        loop->out_queues = queue;
    }

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    event->fiber = vm::GetFiber();

    queue->AddEvent(event, direction);

    return true;
}

void ProcessOutTrigger(EvLoop *loop) {
    std::unique_lock qlock(loop->queue_lock);

    for (EventQueue *queue = loop->out_queues; queue != nullptr; queue = queue->next) {
        std::unique_lock _(queue->lock);
        ProcessQueue(queue, EventDirection::OUT);
    }

    loop->out_queues = nullptr;
}

void ProcessQueue(EventQueue *queue, EventDirection direction) {
    Event **head = &queue->in_event.head;
    bool ok;

    if (direction == EventDirection::OUT)
        head = &queue->out_event.head;

    do {
        thlocal_event = *head;

        if (*head == nullptr)
            break;

        ok = thlocal_event->callback(thlocal_event);
        if (!ok && !argon::vm::IsPanicking())
            return;

        Spawn(thlocal_event->fiber);

        EventDel(queue->PopEvent(direction));
    } while (ok);
}

#endif