// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>

#include <argon/vm/loop2/evloop.h>

using namespace argon::vm;
using namespace argon::vm::datatype;
using namespace argon::vm::loop2;

static EvLoop default_event_loop{};

thread_local Fiber *argon::vm::loop2::evloop_cur_fiber = nullptr;

unsigned long long argon::vm::loop2::TimeNow() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void EvLoopDispatcher(EvLoop *loop) {
    Event *event;

    while (!loop->should_stop) {
        if (loop->io_count == 0) {
            std::unique_lock lock(loop->lock);

            loop->cond.wait(lock, [loop]() {
                return loop->should_stop || loop->io_count > 0;
            });

            lock.unlock();

            if (loop->should_stop)
                break;
        }

        auto loop_time = TimeNow();
        auto timeout = (long) kEventTimeout;

        loop->lock.lock();
        event = loop->event_heap.PopMin();
        loop->lock.unlock();

        if (event != nullptr) {
            timeout = (long) (event->timeout - loop_time);
            if (timeout < 0)
                timeout = 0;
        }

#ifndef _ARGON_PLATFORM_WINDOWS
        // TODO: check ProcessOutQueue(loop);
#endif

        EvLoopIOPoll(loop, timeout);

        while (event != nullptr) {
            if (loop_time < event->timeout) {
                loop->lock.lock();
                loop->event_heap.Insert(event);
                loop->lock.unlock();

                break;
            }

            loop->io_count--;

            argon::vm::Spawn(event->fiber);

            EventDel(event);

            loop->lock.lock();
            event = loop->event_heap.PopMin();
            loop->lock.unlock();
        }
    }
}

// PUBLIC

bool argon::vm::loop2::EvLoopInitRun() {
    if (!EvLoopInit(&default_event_loop))
        return false;

    std::thread(EvLoopDispatcher, &default_event_loop).detach();

    return true;
}

Event *argon::vm::loop2::EventNew(EvLoop *loop, ArObject *initiator) {
    Event *event;

    if (loop == nullptr)
        return nullptr;

    loop->lock.lock();

    event = loop->free_events.Pop();

    loop->lock.unlock();

    if (event == nullptr) {
        event = (Event *) memory::Calloc(sizeof(Event));
        if (event == nullptr)
            return nullptr;
    }

    // TODO: check event->loop = loop;

    event->initiator = IncRef(initiator);

    return event;
}

EvLoop *argon::vm::loop2::EvLoopGet() {
    return &default_event_loop;
}

#ifndef _ARGON_PLATFORM_WINDOWS

EvLoopQueue *argon::vm::loop2::EvLoopQueueNew(EvHandle handle) {
    EvLoopQueue *evLoopQueue;

    if ((evLoopQueue = (EvLoopQueue *) argon::vm::memory::Calloc(sizeof(EvLoopQueue))) == nullptr)
        return nullptr;

    new(&(evLoopQueue->lock))std::mutex();

    evLoopQueue->handle = handle;

    return evLoopQueue;
}

#endif

void argon::vm::loop2::EventDel(Event *event) {
    auto *loop = &default_event_loop;

    Release(event->aux);
    Release(event->initiator);

    BufferRelease(&event->buffer.arbuf);

    std::unique_lock _(loop->lock);

    if (loop->free_events.Count() + 1 <= kMaxFreeEvents) {
        loop->free_events.Push(event);

        return;
    }

    _.unlock();

    argon::vm::memory::Free(event);
}

void argon::vm::loop2::EvLoopShutdown() {
    default_event_loop.should_stop = true;
}

#ifndef _ARGON_PLATFORM_WINDOWS

void argon::vm::loop2::EvLoopQueueDel(EvLoopQueue **ev_queue) {
    auto *queue = *ev_queue;

    Event *event;

    while ((event = queue->in_events.Dequeue()) != nullptr)
        EventDel(event);

    while ((event = queue->out_events.Dequeue()) != nullptr)
        EventDel(event);

    queue->lock.~mutex();

    argon::vm::memory::Free(queue);

    *ev_queue = nullptr;
}

void argon::vm::loop2::EvLoopProcessOut(EvLoop *loop) {
    // TODO: Check this
}

void argon::vm::loop2::EvLoopProcessEvents(EvLoop *loop, EvLoopQueue *ev_queue, EvLoopQueueDirection direction) {
    auto *queue = &ev_queue->in_events;
    CallbackStatus status;

    if (direction == EvLoopQueueDirection::OUT)
        queue = &ev_queue->out_events;

    do {
        std::unique_lock qlck(ev_queue->lock);

        auto *event = queue->Dequeue();

        qlck.unlock();

        if (event == nullptr)
            break;

        evloop_cur_fiber = event->fiber;

        status = event->callback(event);
        if (status == CallbackStatus::RETRY) {
            qlck.lock();

            queue->Enqueue(event);

            return;
        }

        if (status != CallbackStatus::CONTINUE)
            Spawn(event->fiber);

        loop->io_count--;

        EventDel(event);
    } while (status != CallbackStatus::FAILURE);
}

#endif
