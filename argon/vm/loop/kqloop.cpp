// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_DARWIN

#include <sys/event.h>

#include <argon/vm/runtime.h>

#include <argon/vm/loop/event.h>
#include <argon/vm/loop/evloop.h>

using namespace argon::vm::loop;

EvLoop *argon::vm::loop::EventLoopNew() {
    EvLoop *evl;

    if ((evl = (EvLoop *) argon::vm::memory::Calloc(sizeof(EvLoop))) != nullptr) {
        if ((evl->handle = kqueue()) < 0) {
            datatype::ErrorFromErrno(errno);

            argon::vm::memory::Free(evl);

            return nullptr;
        }

        new(&evl->lock)std::mutex();
        new(&evl->out_lock)std::mutex();
        new(&evl->cond)std::condition_variable();
    }

    return evl;
}

bool argon::vm::loop::EventLoopIOPoll(EvLoop *loop, unsigned long timeout) {
    struct kevent events[kMaxEvents];
    struct kevent kev[2];
    timespec ts{};

    ProcessOutQueue(loop);

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

        if (events[i].filter == EVFILT_READ)
            ProcessQueueEvents(loop, queue, EventDirection::IN);
        else if (events[i].filter == EVFILT_WRITE)
            ProcessQueueEvents(loop, queue, EventDirection::OUT);

        std::unique_lock _(queue->lock);

        if (queue->in_events.Count() == 0 && queue->out_events.Count() == 0) {
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

    if (queue->in_events.Count() == 0 && queue->out_events.Count() == 0) {
        EV_SET(kev, queue->handle, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, queue);
        EV_SET(kev + 1, queue->handle, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, queue);

        if (kevent(loop->handle, kev, 2, nullptr, 0, nullptr) < 0) {
            _.unlock();

            vm::SetFiberStatus(FiberStatus::RUNNING);

            datatype::ErrorFromErrno(errno);

            return false;
        }
    }

    auto is_empty = queue->out_events.Count() == 0;

    event->fiber = vm::GetFiber();

    if (direction == EventDirection::IN)
        queue->in_events.Enqueue(event);
    else
        queue->out_events.Enqueue(event);

    if (direction == EventDirection::OUT && is_empty) {
        std::unique_lock out_lock(loop->out_lock);

        queue->next = loop->out_queues;
        loop->out_queues = queue;
    }

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    loop->io_count++;

    loop->cond.notify_one();

    return true;
}

#endif