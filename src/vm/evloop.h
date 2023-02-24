// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_EVLOOP_H_
#define ARGON_VM_EVLOOP_H_

#include <util/macros.h>

#include <vm/datatype/arobject.h>

namespace argon::vm {
#ifdef _ARGON_PLATFORM_WINDOWS

    using EvHandle = void *;

#else

    using EvHandle = int;

#endif

    struct Event;

    struct EvLoop {
        EvHandle handle;

        int event_size;
    };

    Event *EventAlloc(EvLoop *loop, datatype::ArObject *initiator);

    EvLoop *EvLoopNew();

    EvLoop *EvLoopNew(int event_size);

    bool EventPool(EvLoop *loop, Event **out_event, unsigned long timeout);

    bool EvLoopRegister(EvLoop *loop, EvHandle handle);

    void EventDel(Event *event);

} // namespace argon::vm

#endif // !ARGON_VM_EVLOOP_H_
