// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/nil.h>

#include <argon/vm/loop/event.h>
#include <argon/vm/loop/evloop.h>

using namespace argon::vm;
using namespace argon::vm::loop;
using namespace argon::vm::datatype;

EvLoop *default_event_loop = nullptr;

thread_local Fiber *argon::vm::loop::evloop_cur_fiber = nullptr;

// Prototypes

void TimerTaskDel(EvLoop *loop, TimerTask *task);

// Internal

unsigned long long TimeNow() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void EventLoopDispatcher(EvLoop *loop) {
    TimerTask *task;

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

        if ((task = loop->timer_heap.PeekMin()) != nullptr) {
            timeout = (long) (task->timeout - loop_time);
            if (timeout < 0)
                timeout = 0;
        }

        EventLoopIOPoll(loop, timeout);

        while (task != nullptr) {
            if (loop_time < task->timeout)
                break;

            loop->timer_heap.PopMin();

            loop->io_count--;

            argon::vm::Spawn(task->fiber);

            TimerTaskDel(loop, task);

            task = loop->timer_heap.PeekMin();
        }
    }
}

void TimerTaskDel(EvLoop *loop, TimerTask *task) {
    std::unique_lock _(loop->lock);

    if (loop->free_t_tasks.Count() + 1 <= kMaxFreeTasks) {
        loop->free_t_tasks.Push(task);

        return;
    }

    argon::vm::memory::Free(task);
}

// Public

bool argon::vm::loop::EventLoopInit() {
    if ((default_event_loop = EventLoopNew()) == nullptr)
        return false;

    std::thread(EventLoopDispatcher, default_event_loop).detach();

    return true;
}

Event *argon::vm::loop::EventNew(EvLoop *loop, ArObject *initiator) {
    Event *event;

    if (loop == nullptr)
        return nullptr;

    std::unique_lock _(loop->lock);

    event = loop->free_events.Pop();

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

    queue->handle = handle;

    return queue;
}

#endif

EvLoop *argon::vm::loop::GetEventLoop() {
    return default_event_loop;
}

bool argon::vm::loop::EventLoopSetTimeout(EvLoop *loop, datatype::ArSize timeout) {
    TimerTask *task;

    if (loop == nullptr)
        return false;

    auto now = TimeNow();

    std::unique_lock _(loop->lock);

    task = (TimerTask *) loop->free_t_tasks.Pop();

    _.unlock();

    if (task == nullptr) {
        task = (TimerTask *) memory::Alloc(sizeof(TimerTask));
        if (task == nullptr)
            return false;
    }

    memory::MemoryZero(task, sizeof(TimerTask));

    task->fiber = vm::GetFiber();

    task->callback = [](Task *_task) {
        vm::FiberSetAsyncResult(_task->fiber, (ArObject *) Nil);
    };

    _.lock();

    task->id = loop->t_task_id++;
    task->timeout = now + timeout;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    loop->timer_heap.Insert(task);

    loop->io_count++;

    loop->cond.notify_one();

    return true;
}

void argon::vm::loop::EventDel(Event *event) {
    auto *loop = event->loop;

    Release(event->initiator);
    Release(event->aux);

    BufferRelease(&event->buffer.arbuf);

    std::unique_lock _(loop->lock);

    if (loop->free_events.Count() + 1 <= kMaxFreeEvents) {
        loop->free_events.Push(event);

        return;
    }

    _.unlock();

    argon::vm::memory::Free(event);
}

void argon::vm::loop::EventLoopShutdown() {
    auto *loop = GetEventLoop();

    if (loop != nullptr)
        loop->should_stop = true;
}

#ifndef _ARGON_PLATFORM_WINDOWS

void argon::vm::loop::EventQueueDel(EventQueue **queue) {
    (*queue)->lock.~mutex();

    argon::vm::memory::Free(*queue);

    *queue = nullptr;
}

void argon::vm::loop::ProcessOutQueue(argon::vm::loop::EvLoop *loop) {
    std::unique_lock _(loop->out_lock);

    for (EventQueue *queue = loop->out_queues; queue != nullptr; queue = queue->next)
        ProcessQueueEvents(loop, queue, EventDirection::OUT);

    loop->out_queues = nullptr;
}

void argon::vm::loop::ProcessQueueEvents(EvLoop *loop, EventQueue *queue, EventDirection direction) {
    auto *ev_queue = &queue->in_events;

    if (direction == EventDirection::OUT)
        ev_queue = &queue->out_events;

    CallbackStatus status;

    do {
        auto *event = ev_queue->GetHead();
        if (event == nullptr)
            break;

        evloop_cur_fiber = event->fiber;

        status = event->callback(event);
        if (status == CallbackStatus::RETRY)
            return;

        if (status != CallbackStatus::CONTINUE)
            Spawn(event->fiber);

        loop->io_count--;

        std::unique_lock _(queue->lock);

        event = ev_queue->Dequeue();

        _.unlock();

        EventDel(event);
    } while (status != CallbackStatus::FAILURE);
}

#endif