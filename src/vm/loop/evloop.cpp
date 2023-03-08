// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>

#include <vm/runtime.h>

#include <vm/datatype/nil.h>

#include "event.h"
#include "evloop.h"

using namespace argon::vm;
using namespace argon::vm::loop;
using namespace argon::vm::datatype;

EvLoop *default_event_loop = nullptr;

thread_local Event *argon::vm::loop::thlocal_event = nullptr;

// Prototypes

void TimerTaskDel(TimerTask *ttask);

// Internal

unsigned long long TimeNow() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void EventLoopDispatcher(EvLoop *loop) {
    TimerTask *ttask;

    unsigned long long loop_time;
    long timeout;

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

        loop_time = TimeNow();

        timeout = kEventTimeout;
        if ((ttask = loop->timer_heap.PeekMin()) != nullptr) {
            timeout = (long) (ttask->timeout - loop_time);
            if (timeout < 0)
                timeout = 0;
        }

        EventLoopIOPoll(loop, timeout);

        while (ttask != nullptr) {
            if (loop_time < ttask->timeout)
                break;

            loop->timer_heap.PopMin();

            loop->io_count--;

            argon::vm::Spawn(ttask->fiber);

            TimerTaskDel(ttask);

            ttask = loop->timer_heap.PeekMin();
        }
    }
}

void TimerTaskDel(TimerTask *ttask) {
    auto *loop = ttask->loop;

    std::unique_lock _(loop->lock);

    if (loop->free_t_task_count + 1 <= kMaxFreeTasks) {
        ttask->next = loop->free_t_task;
        loop->free_t_task = ttask;

        loop->free_t_task_count++;

        return;
    }

    argon::vm::memory::Free(ttask);
}

// Public

bool argon::vm::loop::EventLoopInit() {
    if ((default_event_loop = EventLoopNew()) == nullptr)
        return false;

    std::thread(EventLoopDispatcher, default_event_loop).detach();

    return true;
}

Event *argon::vm::loop::EventNew(EvLoop *loop, ArObject *initiator) {
    Event *event = nullptr;

    if (loop == nullptr)
        return nullptr;

    std::unique_lock _(loop->lock);

    if (loop->free_events != nullptr) {
        event = loop->free_events;
        loop->free_events = event->next;

        loop->free_events_count--;
    }

    _.unlock();

    if (event == nullptr) {
        event = (Event *) memory::Alloc(sizeof(Event));
        if (event == nullptr)
            return nullptr;
    }

    memory::MemoryZero(event, sizeof(Event));

    event->loop = loop;

    event->initiator = IncRef(initiator);

    return event;
}

#ifndef _ARGON_PLATFORM_WINDOWS

EventQueue *argon::vm::loop::EventQueueNew(EvHandle handle) {
    EventQueue *queue;

    if ((queue = (EventQueue *) argon::vm::memory::Calloc(sizeof(EventQueue))) == nullptr)
        return nullptr;

    new(&(queue->lock))std::mutex();

    queue->items = 0;
    queue->handle = handle;

    return queue;
}

#endif

EvLoop *argon::vm::loop::GetEventLoop() {
    return default_event_loop;
}

bool argon::vm::loop::EventLoopSetTimeout(EvLoop *loop, datatype::ArSize timeout) {
    auto now = TimeNow();
    TimerTask *ttask = nullptr;

    if (loop == nullptr)
        return false;

    std::unique_lock _(loop->lock);

    if (loop->free_t_task != nullptr) {
        ttask = loop->free_t_task;
        loop->free_t_task = (TimerTask *) ttask->next;

        loop->free_t_task_count--;
    }

    _.unlock();

    if (ttask == nullptr) {
        ttask = (TimerTask *) memory::Alloc(sizeof(TimerTask));
        if (ttask == nullptr)
            return false;
    }

    memory::MemoryZero(ttask, sizeof(TimerTask));

    ttask->loop = loop;
    ttask->fiber = vm::GetFiber();

    ttask->callback = [](Task *task) {
        vm::FiberSetAsyncResult(task->fiber, (ArObject *) Nil);
    };

    _.lock();

    ttask->id = loop->t_task_id++;
    ttask->timeout = now + timeout;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    loop->timer_heap.Insert(ttask);

    loop->io_count++;

    return true;
}

void argon::vm::loop::EventDel(Event *event) {
    auto *loop = event->loop;

    Release(event->initiator);
    Release(event->aux);

    std::unique_lock _(loop->lock);

    if (loop->free_events_count + 1 <= kMaxFreeEvents) {
        event->next = loop->free_events;
        loop->free_events = event;

        loop->free_events_count++;

        return;
    }

    argon::vm::memory::Free(event);
}

void argon::vm::loop::EventLoopShutdown() {
    auto *loop = GetEventLoop();

    if (loop != nullptr)
        loop->should_stop = true;
}

#ifndef _ARGON_PLATFORM_WINDOWS

void argon::vm::loop::EventQueueDel(EventQueue **queue) {
    assert((*queue)->items == 0);

    (*queue)->lock.~mutex();

    argon::vm::memory::Free(*queue);

    *queue = nullptr;
}

// EventQueue
Event *argon::vm::loop::EventQueue::PopEvent(EventDirection direction) {
    Event **head = &this->in_event.head;
    Event **tail = &this->in_event.tail;
    Event *ret;

    if (direction == EventDirection::OUT) {
        head = &this->out_event.head;
        tail = &this->out_event.tail;
    }

    ret = *head;

    if (ret != nullptr) {
        if (ret->prev != nullptr)
            ret->prev->next = nullptr;

        *head = ret->prev;

        if (*tail == ret)
            *tail = nullptr;

        this->items--;
    }

    return ret;
}

void argon::vm::loop::EventQueue::AddEvent(Event *event, EventDirection direction) {
    Event **head = &this->in_event.head;
    Event **tail = &this->in_event.tail;

    if (direction == EventDirection::OUT) {
        head = &this->out_event.head;
        tail = &this->out_event.tail;
    }

    event->next = *tail;
    event->prev = nullptr;

    *tail = event;

    if (*head == nullptr)
        *head = event;

    this->items++;
}

#endif