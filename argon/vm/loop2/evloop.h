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

        std::atomic_uint timer_count;

        EvHandle handle;

        unsigned int time_id;

        bool should_stop;
    };

    extern thread_local struct Fiber *evloop_cur_fiber;

    bool AddEvent(EvLoop *loop, EvLoopQueue *ev_queue, Event *event, EvLoopQueueDirection direction,
                  unsigned int timeout);

    inline bool AddEvent(EvLoop *loop, EvLoopQueue *ev_queue, Event *event, EvLoopQueueDirection direction) {
        return AddEvent(loop, ev_queue, event, direction, 0);
    }

    bool EvLoopInitRun();

    bool EvLoopInit(EvLoop *loop);

    bool IOPoll(EvLoop *loop, unsigned long timeout);

    bool SetTimeout(EvLoop *loop, unsigned long long timeout);

    Event *EventNew(EvLoop *loop, datatype::ArObject *initiator);

    EvLoop *EvLoopGet();

#ifndef _ARGON_PLATFORM_WINDOWS

    EvLoopQueue *QueueNew(EvHandle handle);

#endif

    unsigned long long TimeNow();

    void EventDel(Event *event);

    void Shutdown();

#ifndef _ARGON_PLATFORM_WINDOWS

    void QueueDel(EvLoopQueue **ev_queue);

    void ProcessEvents(EvLoop *loop, EvLoopQueue *ev_queue, EvLoopQueueDirection direction);

#endif

} // namespace argon::vm::loop2

#endif // !ARGON_VM_LOOP2_EVLOOP_H_
