// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_EVLOOP_H_
#define ARGON_VM_LOOP_EVLOOP_H_

#include <mutex>

#include <util/macros.h>

#include <vm/datatype/arobject.h>

#include "event.h"

namespace argon::vm::loop {
    constexpr const unsigned int kEventTimeout = 500; // millisecond
    constexpr const unsigned int kMaxFreeEvents = 2046;

#ifdef _ARGON_PLATFORM_WINDOWS

    using EvHandle = void *;

#else
    constexpr const unsigned int kMaxEvents = 50;

    using EvHandle = int;

    enum class EventDirection {
        IN,
        OUT
    };

    struct EventQueue {
        std::mutex lock;

        EventQueue *next;

        struct {
            Event *head;
            Event *tail;
        } in_event;

        struct {
            Event *head;
            Event *tail;
        } out_event;

        unsigned int items;

        EvHandle handle;

        Event *PopEvent(EventDirection direction);

        void AddEvent(Event *event, EventDirection direction);
    };

#endif

    struct EvLoop {
        std::mutex queue_lock;

#ifndef _ARGON_PLATFORM_WINDOWS
        EventQueue *out_queues;
#endif

        Event *allocable_events;

        datatype::ArSize free_count;

        EvHandle handle;
    };

    extern thread_local Event *thlocal_event;

    Event *EventNew(EvLoop *loop, datatype::ArObject *initiator);

#ifndef _ARGON_PLATFORM_WINDOWS

    EventQueue *EventQueueNew(EvHandle handle);

#endif

    EvLoop *EventLoopNew();

    EvLoop *GetEventLoop();

#ifdef _ARGON_PLATFORM_WINDOWS

    bool EventLoopAddHandle(EvLoop *loop, EvHandle handle);

#else

    bool EventLoopAddEvent(EvLoop *loop, EventQueue *queue, Event *event, EventDirection direction);

#endif

    bool EventLoopInit();

    bool EventLoopIOPoll(EvLoop *loop, unsigned long timeout);

    void EventDel(Event *event);

    void EventLoopShutdown();

#ifndef _ARGON_PLATFORM_WINDOWS

    void EventQueueDel(EventQueue **queue);

#endif

} // namespace argon::vm::loop

#endif // !ARGON_VM_LOOP_EVLOOP_H_
