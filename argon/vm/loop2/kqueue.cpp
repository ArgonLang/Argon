// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_DARWIN

#include <sys/event.h>

#include <argon/vm/runtime.h>

#include <argon/vm/loop2/evloop.h>

using namespace argon::vm::loop2;

bool argon::vm::loop2::AddEvent(EvLoop *loop, EvLoopQueue *ev_queue, Event *event, EvLoopQueueDirection direction,
                                unsigned int timeout) {
    struct kevent kev[2];
    int changes = 0;

    bool in_selected = false;

    event->fiber = vm::GetFiber();

    std::unique_lock _(ev_queue->lock);

    if (direction == EvLoopQueueDirection::IN && !ev_queue->in_set) {
        EV_SET(kev, ev_queue->handle, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ev_queue);
        ev_queue->in_set = true;
        in_selected = true;
        changes++;
    }

    if (direction == EvLoopQueueDirection::OUT && !ev_queue->out_set) {
        EV_SET(kev + changes, ev_queue->handle, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ev_queue);
        ev_queue->out_set = true;
        changes++;
    }

    if (changes > 0) {
        auto res = kevent(loop->handle, kev, changes, nullptr, 0, nullptr);
        if (res < 0) {
            if (changes > 1) {
                ev_queue->in_set = false;
                ev_queue->out_set = false;
            } else {
                if (in_selected)
                    ev_queue->in_set = false;
                else
                    ev_queue->out_set = false;
            }

            _.unlock();

            vm::SetFiberStatus(FiberStatus::RUNNING);

            datatype::ErrorFromErrno(errno);

            return false;
        }
    }

    if (timeout > 0) {
        loop->lock.lock();

        event->timeout = TimeNow() + timeout;
        event->id = loop->time_id++;
        event->discard_on_timeout = true;
        event->refc++;

        loop->event_heap.Insert(event);

        loop->lock.unlock();

        loop->timer_count++;
    }

    if (direction == EvLoopQueueDirection::IN)
        ev_queue->in_events.Enqueue(event);
    else
        ev_queue->out_events.Enqueue(event);

    _.unlock();

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    loop->io_count++;

    loop->cond.notify_one();

    return true;
}

bool argon::vm::loop2::EvLoopInit(EvLoop *loop) {
    if ((loop->handle = kqueue()) < 0) {
        datatype::ErrorFromErrno(errno);

        return false;
    }

    new(&loop->lock)std::mutex();
    new(&loop->cond)std::condition_variable();

    return true;
}

bool argon::vm::loop2::IOPoll(EvLoop *loop, unsigned long timeout) {
    struct kevent events[kMaxEvents];
    struct kevent kev[2];

    timespec ts{};
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
        auto *ev_queue = (EvLoopQueue *) events[i].udata;

        if (events[i].filter == EVFILT_READ)
            ProcessEvents(loop, ev_queue, EvLoopQueueDirection::IN);
        else if (events[i].filter == EVFILT_WRITE)
            ProcessEvents(loop, ev_queue, EvLoopQueueDirection::OUT);

        std::unique_lock _(ev_queue->lock);

        int changes = 0;
        if (events[i].filter == EVFILT_READ && ev_queue->in_events.Count() == 0) {
            EV_SET(kev, ev_queue->handle, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            ev_queue->in_set = false;
            changes++;
        }

        if (events[i].filter == EVFILT_WRITE && ev_queue->out_events.Count() == 0) {
            EV_SET(kev + changes, ev_queue->handle, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
            ev_queue->out_set = false;
            changes++;
        }

        if (changes > 0 && kevent(loop->handle, kev, changes, nullptr, 0, nullptr) < 0)
            assert(false); // Never get here!
    }

    return true;
}

#endif
