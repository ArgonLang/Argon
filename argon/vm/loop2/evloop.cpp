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
        if (loop->io_count == 0 && loop->timer_count == 0) {
            std::unique_lock lock(loop->lock);

            loop->cond.wait(lock, [loop]() {
                return loop->should_stop || loop->io_count > 0 || loop->timer_count > 0;
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

        IOPoll(loop, timeout);

        while (event != nullptr) {
            std::unique_lock elck(event->lock);

            if (event->timeout > 0) {
                if (loop_time < event->timeout) {
                    elck.unlock();

                    loop->lock.lock();
                    loop->event_heap.Insert(event);
                    loop->lock.unlock();

                    break;
                }

                if (event->discard_on_timeout) {
                    evloop_cur_fiber = event->fiber;
                    ErrorFormat(kTimeoutError[0], "IO operation on '%s' did not complete within the required time",
                                event->initiator);

                    event->timeout = 0;

                    if (event->user_callback != nullptr)
                        event->user_callback(event, event->aux, ETIMEDOUT);
                }

                elck.unlock();

                argon::vm::Spawn(event->fiber);
            }

            loop->timer_count--;

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

bool argon::vm::loop2::SetTimeout(EvLoop *loop, unsigned long long timeout) {
    auto now = TimeNow();

    auto *event = EventNew(loop, nullptr);
    if (event == nullptr)
        return false;

    vm::SetFiberStatus(FiberStatus::BLOCKED);

    event->fiber = vm::GetFiber();
    /*
    event->callback = [](Event *event) -> CallbackStatus {
        vm::FiberSetAsyncResult(event->fiber, nullptr);
        return CallbackStatus::SUCCESS;
    };
     */
    event->timeout = now + timeout;

    loop->lock.lock();

    event->id = loop->time_id++;

    loop->event_heap.Insert(event);

    loop->lock.unlock();

    loop->timer_count++;

    loop->cond.notify_one();

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

        new(&event->lock)std::mutex();
    }

    event->initiator = IncRef(initiator);

    event->refc = 1;

    return event;
}

EvLoop *argon::vm::loop2::EvLoopGet() {
    return &default_event_loop;
}

#ifndef _ARGON_PLATFORM_WINDOWS

EvLoopQueue *argon::vm::loop2::QueueNew(EvHandle handle) {
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

    if (event->refc.fetch_sub(1) > 1)
        return;

    Release(event->aux);
    Release(event->initiator);

    BufferRelease(&event->buffer.arbuf);

    std::unique_lock _(loop->lock);

    if (loop->free_events.Count() + 1 <= kMaxFreeEvents) {
        event->callback = nullptr;
        event->user_callback = nullptr;

        event->aux = nullptr;
        event->initiator = nullptr;

        event->timeout = 0;
        event->id = 0;
        event->discard_on_timeout = false;

        loop->free_events.Push(event);

        return;
    }

    _.unlock();

    event->lock.~mutex();

    argon::vm::memory::Free(event);
}

void argon::vm::loop2::Shutdown() {
    default_event_loop.should_stop = true;

    default_event_loop.cond.notify_all();
}

#ifndef _ARGON_PLATFORM_WINDOWS

void argon::vm::loop2::QueueDel(EvLoopQueue **ev_queue) {
    auto *queue = *ev_queue;

    Event *event;

    while ((event = queue->in_events.Dequeue()) != nullptr)
        EventDel(event);

    while ((event = queue->out_events.Dequeue()) != nullptr)
        EventDel(event);

    printf("Chiusa coda: %d\n", (*ev_queue)->handle);

    queue->lock.~mutex();

    argon::vm::memory::Free(queue);

    *ev_queue = nullptr;
}

void argon::vm::loop2::ProcessEvents(EvLoop *loop, EvLoopQueue *ev_queue, EvLoopQueueDirection direction) {
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

        std::unique_lock elck(event->lock);

        status = CallbackStatus::SUCCESS;
        if (!event->discard_on_timeout || event->timeout > 0) {
            evloop_cur_fiber = event->fiber;

            status = event->callback(event);
            if (status == CallbackStatus::RETRY) {
                qlck.lock();

                queue->Enqueue(event);

                return;
            }

            if (status != CallbackStatus::CONTINUE)
                Spawn(event->fiber);

            event->timeout = 0;
        }

        elck.unlock();

        loop->io_count--;

        EventDel(event);
    } while (status != CallbackStatus::FAILURE);
}

#endif
