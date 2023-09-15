// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_EVLOOP_H_
#define ARGON_VM_LOOP_EVLOOP_H_

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <argon/util/macros.h>

#include <argon/vm/datatype/arobject.h>

#include <argon/vm/loop/evqueue.h>
#include "argon/vm/loop/support/minheap.h"
#include <argon/vm/loop/task.h>

namespace argon::vm::loop {
    constexpr const unsigned int kEventTimeout = 24; // millisecond
    constexpr const unsigned int kMaxFreeEvents = 2046;
    constexpr const unsigned int kMaxFreeTasks = 128;

    struct EvLoop {
        std::mutex lock;

#ifndef _ARGON_PLATFORM_WINDOWS
        std::mutex out_lock;
#endif

        std::condition_variable cond;

        support::MinHeap<TimerTask, TimerTaskLess> timer_heap;

#ifndef _ARGON_PLATFORM_WINDOWS
        EventQueue *out_queues;
#endif

        Event *free_events;

        TimerTask *free_t_task;

        datatype::ArSize free_events_count;

        datatype::ArSize free_t_task_count;

        datatype::ArSize t_task_id;

        std::atomic_ulong io_count;

        EvHandle handle;

        bool should_stop;
    };

    extern thread_local Event *thlocal_event;

    Event *EventNew(EvLoop *loop, datatype::ArObject *initiator);

#ifndef _ARGON_PLATFORM_WINDOWS

    EventQueue *EventQueueNew(EvHandle handle);

#endif

    EvLoop *EventLoopNew();

    EvLoop *GetEventLoop();

#ifdef _ARGON_PLATFORM_WINDOWS

    bool EventLoopAddEvent(EvLoop *loop, Event *event);

    bool EventLoopAddHandle(EvLoop *loop, EvHandle handle);

#else

    bool EventLoopAddEvent(EvLoop *loop, EventQueue *queue, Event *event, EventDirection direction);

#endif

    bool EventLoopInit();

    bool EventLoopIOPoll(EvLoop *loop, unsigned long timeout);

    bool EventLoopSetTimeout(EvLoop *loop, datatype::ArSize timeout);

    void EventDel(Event *event);

    void EventLoopShutdown();

#ifndef _ARGON_PLATFORM_WINDOWS

    void EventQueueDel(EventQueue **queue);

#endif

} // namespace argon::vm::loop

#endif // !ARGON_VM_LOOP_EVLOOP_H_
