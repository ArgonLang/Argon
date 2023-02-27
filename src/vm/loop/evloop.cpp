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

Event *argon::vm::loop::EventAlloc(EvLoop *loop, ArObject *initiator) {
    if (loop == nullptr)
        return nullptr;

    auto *event = (Event *) memory::Alloc(sizeof(Event));

    assert(event != nullptr);

    memory::MemoryZero(event, sizeof(Event));

    event->loop = loop;

    event->initiator = IncRef(initiator);

    return event;
}

EvLoop *argon::vm::loop::GetEventLoop() {
    return default_event_loop;
}

void argon::vm::loop::EventDel(Event *event) {
    Release(event->initiator);

    memory::Free(event);
}

void argon::vm::loop::EventLoopShutdown() {
    evloop_should_stop = true;
}
