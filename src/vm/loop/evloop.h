// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_EVLOOP_H_
#define ARGON_VM_LOOP_EVLOOP_H_

#include <util/macros.h>

#include <vm/datatype/arobject.h>

#include "event.h"

namespace argon::vm::loop {
    constexpr const unsigned int kEventTimeout = 500; // millisecond

#ifdef _ARGON_PLATFORM_WINDOWS

    using EvHandle = void *;

#else

    using EvHandle = int;

#endif

    extern thread_local Event *thlocal_event;

    struct EvLoop {
        EvHandle handle;
    };

    Event *EventAlloc(EvLoop *loop, datatype::ArObject *initiator);

    EvLoop *EventLoopNew();

    EvLoop *GetEventLoop();

    bool EventLoopInit();

    bool EventLoopIOPoll(EvLoop *loop, unsigned long timeout);

    bool EventLoopIOAdd(EvLoop * loop, EvHandle handle);

    void EventDel(Event *event);

    void EventLoopShutdown();

} // namespace argon::vm::loop

#endif // !ARGON_VM_LOOP_EVLOOP_H_
