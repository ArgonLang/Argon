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

// Prototypes

void ProcessOutTrigger(EvLoop *loop);

void ProcessQueue(EventQueue *queue, EventDirection direction);

// EOL

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

    ProcessOutTrigger(loop);

    auto ret = epoll_wait(loop->handle, events, kMaxEvents, (int) timeout);
    if (ret < 0) {
        if (errno == EINTR)
            return false; // Try again

        // Never get here!
        assert(false);
    }

    for (int i = 0; i < ret; i++) {
        auto *queue = (EventQueue *) events[i].data.ptr;

        std::unique_lock _(queue->lock);

        if (events[i].events & EPOLLIN)
            ProcessQueue(queue, EventDirection::IN);

        if (events[i].events & EPOLLOUT)
            ProcessQueue(queue, EventDirection::OUT);

        if (queue->items == 0 && epoll_ctl(loop->handle, EPOLL_CTL_DEL, queue->handle, nullptr) < 0)
            assert(false); // Never get here!
    }

    return true;
}

bool argon::vm::loop::EventLoopAddEvent(EvLoop *loop, EventQueue *queue, Event *event, EventDirection direction) {
    epoll_event ep_event{};

    std::unique_lock _(queue->lock);

    if (queue->items == 0) {
        ep_event.events = EPOLLOUT | EPOLLIN | EPOLLET;
        ep_event.data.ptr = queue;

        if (epoll_ctl(loop->handle, EPOLL_CTL_ADD, queue->handle, &ep_event) < 0) {
            _.unlock();

            vm::SetFiberStatus(FiberStatus::RUNNING);

            ErrorFromErrno(errno);

            return false;
        }
    } else if (direction == EventDirection::OUT && queue->out_event.head == nullptr) {
        // Since an event with active EPOLLET flag is used, it is possible that the send callback
        // is not immediately invoked, to avoid this, the desire to write to a socket is signaled
        // in a separate queue
        std::unique_lock out_lock(loop->out_lock);

        queue->next = loop->out_queues;
        loop->out_queues = queue;
    }

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    event->fiber = vm::GetFiber();

    queue->AddEvent(event, direction);

    loop->io_count++;

    loop->cond.notify_one();

    return true;
}

void ProcessOutTrigger(EvLoop *loop) {
    std::unique_lock _(loop->out_lock);

    for (EventQueue *queue = loop->out_queues; queue != nullptr; queue = queue->next) {
        std::unique_lock qlock(queue->lock);
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

        thlocal_event->loop->io_count--;

        Spawn(thlocal_event->fiber);

        EventDel(queue->PopEvent(direction));
    } while (ok);
}

#endif