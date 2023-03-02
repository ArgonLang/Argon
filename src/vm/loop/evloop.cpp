// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <thread>

#include <vm/runtime.h>

#include "event.h"
#include "evloop.h"

using namespace argon::vm;
using namespace argon::vm::loop;
using namespace argon::vm::datatype;

thread_local Event *argon::vm::loop::thlocal_event = nullptr;

EvLoop *default_event_loop = nullptr;

bool evloop_should_stop = false;

// Internal

void EventLoopDispatcher() {
    while (!evloop_should_stop) {
        EventLoopIOPoll(default_event_loop, kEventTimeout);

        // TODO: Timers
    }
}

// Public

bool argon::vm::loop::EventLoopInit() {
    if ((default_event_loop = EventLoopNew()) == nullptr)
        return false;

    std::thread(EventLoopDispatcher).detach();

    return true;
}

Event *argon::vm::loop::EventNew(EvLoop *loop, ArObject *initiator) {
    if (loop == nullptr)
        return nullptr;

    auto *event = (Event *) memory::Alloc(sizeof(Event));

    assert(event != nullptr);

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

void argon::vm::loop::EventDel(Event *event) {
    Release(event->initiator);
    Release(event->aux);

    memory::Free(event);
}

void argon::vm::loop::EventLoopShutdown() {
    evloop_should_stop = true;
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