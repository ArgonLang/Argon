// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_LINUX

#include <sys/epoll.h>

#include <argon/vm/runtime.h>

#include <argon/vm/loop2/evloop.h>

using namespace argon::vm::loop2;

bool argon::vm::loop2::EvLoopAddEvent(EvLoop *loop, EvLoopQueue *ev_queue, Event *event, EvLoopQueueDirection direction,
                                      unsigned int timeout) {
    epoll_event ep_event{};

    event->fiber = vm::GetFiber();

    std::unique_lock _(ev_queue->lock);

    if (ev_queue->in_events.Count() == 0 && ev_queue->out_events.Count() == 0) {
        ep_event.events = EPOLLOUT | EPOLLIN | EPOLLET;
        ep_event.data.ptr = ev_queue;

        if (epoll_ctl(loop->handle, EPOLL_CTL_ADD, ev_queue->handle, &ep_event) < 0) {
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
    if ((loop->handle = epoll_create1(EPOLL_CLOEXEC)) < 0) {
        datatype::ErrorFromErrno(errno);

        return false;
    }

    new(&loop->lock)std::mutex();
    new(&loop->cond)std::condition_variable();

    return true;
}

bool argon::vm::loop2::EvLoopIOPoll(EvLoop *loop, unsigned long timeout) {
    epoll_event events[kMaxEvents];

    auto ret = epoll_wait(loop->handle, events, kMaxEvents, (int) timeout);
    if (ret < 0) {
        if (errno == EINTR)
            return false; // Try again

        // Never get here!
        assert(false);
    }

    for (int i = 0; i < ret; i++) {
        auto *ev_queue = (EvLoopQueue *) events[i].data.ptr;

        if (events[i].events & EPOLLIN)
            EvLoopProcessEvents(loop, ev_queue, EvLoopQueueDirection::IN);

        if (events[i].events & EPOLLOUT)
            EvLoopProcessEvents(loop, ev_queue, EvLoopQueueDirection::OUT);

        std::unique_lock _(ev_queue->lock);

        if (ev_queue->in_events.Count() == 0 && ev_queue->out_events.Count() == 0 &&
            epoll_ctl(loop->handle, EPOLL_CTL_DEL, ev_queue->handle, nullptr) < 0)
            assert(false); // Never get here!
    }

    return true;
}

#endif
