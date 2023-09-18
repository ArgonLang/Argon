// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_LINUX

#include <sys/epoll.h>

#include <argon/vm/runtime.h>

#include <argon/vm/memory/memory.h>

#include <argon/vm/datatype/error.h>

#include <argon/vm/loop/event.h>
#include <argon/vm/loop/evloop.h>

using namespace argon::vm::loop;
using namespace argon::vm::datatype;

EvLoop *argon::vm::loop::EventLoopNew() {
    EvLoop *evl;

    if ((evl = (EvLoop *) argon::vm::memory::Calloc(sizeof(EvLoop))) != nullptr) {
        if ((evl->handle = epoll_create1(EPOLL_CLOEXEC)) < 0) {
            ErrorFromErrno(errno);

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
    epoll_event events[kMaxEvents];

    ProcessOutQueue(loop);

    auto ret = epoll_wait(loop->handle, events, kMaxEvents, (int) timeout);
    if (ret < 0) {
        if (errno == EINTR)
            return false; // Try again

        // Never get here!
        assert(false);
    }

    for (int i = 0; i < ret; i++) {
        auto *queue = (EventQueue *) events[i].data.ptr;

        if (events[i].events & EPOLLIN)
            ProcessQueueEvents(loop, queue, EventDirection::IN);

        if (events[i].events & EPOLLOUT)
            ProcessQueueEvents(loop, queue, EventDirection::OUT);

        std::unique_lock _(queue->lock);

        if (queue->in_events.Count() == 0 && queue->out_events.Count() == 0 &&
            epoll_ctl(loop->handle, EPOLL_CTL_DEL, queue->handle, nullptr) < 0)
            assert(false); // Never get here!
    }

    return true;
}

bool argon::vm::loop::EventLoopAddEvent(EvLoop *loop, EventQueue *queue, Event *event, EventDirection direction) {
    epoll_event ep_event{};

    std::unique_lock _(queue->lock);

    if (queue->in_events.Count() == 0 && queue->out_events.Count() == 0) {
        ep_event.events = EPOLLOUT | EPOLLIN | EPOLLET;
        ep_event.data.ptr = queue;

        if (epoll_ctl(loop->handle, EPOLL_CTL_ADD, queue->handle, &ep_event) < 0) {
            _.unlock();

            vm::SetFiberStatus(FiberStatus::RUNNING);

            ErrorFromErrno(errno);

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
        // Since an event with active EPOLLET flag is used, it is possible that the send callback
        // is not immediately invoked, to avoid this, the desire to write to a socket is signaled
        // in a separate queue
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