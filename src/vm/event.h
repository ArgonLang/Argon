// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_EVENT_H_
#define ARGON_VM_EVENT_H_

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <minwindef.h>
#include <minwinbase.h>
#include <winsock2.h>

#undef CONST
#undef FASTCALL
#undef Yield

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

        struct EvLoop *loop;

        struct Fiber *fiber;

        EventCB callback;

        datatype::ArObject *aux;

        datatype::ArObject *initiator;

        struct {
            datatype::ArBuffer bufferable;

#ifdef _ARGON_PLATFORM_WINDOWS
            WSABUF wsa;
#else
            unsigned char *data;

            datatype::ArSize length;
#endif
            datatype::ArSize allocated;
        } buffer;
    };

} // namespace argon::vm

#endif // !ARGON_VM_EVENT_H_
