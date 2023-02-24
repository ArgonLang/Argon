// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/memory/memory.h>

#include "event.h"
#include "evloop.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

Event *argon::vm::EventAlloc(EvLoop *loop, ArObject *initiator) {
    if(loop == nullptr)
        return nullptr;

    auto *event = (Event *) memory::Alloc(loop->event_size);

    assert(event != nullptr);

    memory::MemoryZero(event, loop->event_size);

    event->loop = loop;

    event->initiator = IncRef(initiator);

    return event;
}

EvLoop *argon::vm::EvLoopNew() {
    return EvLoopNew(sizeof(Event));
}

void argon::vm::EventDel(Event *event) {
    Release(event->initiator);

    memory::Free(event);
}
