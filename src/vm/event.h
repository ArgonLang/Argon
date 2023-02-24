// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_EVENT_H_
#define ARGON_VM_EVENT_H_

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <minwindef.h>
#include <minwinbase.h>

#undef CONST
#undef FASTCALL

#endif

#include <vm/datatype/arobject.h>

namespace argon::vm {

    using EventCB = void (*)(struct Event *);

#ifdef _ARGON_PLATFORM_WINDOWS
    struct Event : OVERLAPPED {
#else
        struct Event {
#endif
        Event *next;
        Event **prev;

        EventCB callback;

        struct EvLoop *loop;

        struct Fiber *fiber;

        datatype::ArObject *initiator;

        unsigned char *buffer;
        size_t length;
    };

} // namespace argon::vm

#endif // !ARGON_VM_EVENT_H_
