// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP2_EVLOOP_H_
#define ARGON_VM_LOOP2_EVLOOP_H_

#include <argon/vm/loop2/support/minheap.h>
#include <argon/vm/loop2/event.h>

namespace argon::vm::loop2 {
    constexpr const unsigned int kEventTimeout = 24;    // millisecond
    constexpr const unsigned int kMaxFreeEvents = 1024;
    constexpr const unsigned int kMaxFreeTasks = 128;

#ifdef _ARGON_PLATFORM_WINDOWS

    using EvHandle = void *;

#else
    constexpr const unsigned int kMaxEvents = 50;

    using EvHandle = int;

    enum class EvLoopQueueDirection {
        IN,
        OUT
    };

    struct EvLoopQueue {
        std::mutex lock;

        EvLoopQueue *next;

        EventQueue in_events;

        EventQueue out_events;

        EvHandle handle;
    };

#endif

    struct EvLoop {
        std::mutex lock;

        std::condition_variable cond;

        support::MinHeap<Event, EventLess> event_heap;

        EventStack free_events;

        std::atomic_uint io_count;

        EvHandle handle;

        bool should_stop;
    };

    extern thread_local struct Fiber *evloop_cur_fiber;

    bool EvLoopAddEvent(EvLoop *loop, EvLoopQueue *ev_queue, Event *event, EvLoopQueueDirection direction);

    bool EvLoopInitRun();

    bool EvLoopInit(EvLoop *loop);

    bool EvLoopIOPoll(EvLoop *loop, unsigned long timeout);

    Event *EventNew(EvLoop *loop, datatype::ArObject *initiator);

    EvLoop *EvLoopGet();

#ifndef _ARGON_PLATFORM_WINDOWS

    EvLoopQueue *EvLoopQueueNew(EvHandle handle);

#endif

    unsigned long long TimeNow();

    void EventDel(Event *event);

    void EvLoopShutdown();

#ifndef _ARGON_PLATFORM_WINDOWS

    void EvLoopQueueDel(EvLoopQueue **ev_queue);

    void EvLoopProcessEvents(EvLoop *loop, EvLoopQueue *ev_queue, EvLoopQueueDirection direction);

#endif

} // namespace argon::vm::loop2

#endif // !ARGON_VM_LOOP2_EVLOOP_H_
